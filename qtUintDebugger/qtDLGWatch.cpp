/*
 * 	This file is part of uintDebugger.
 *
 *    uintDebugger is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    uintDebugger is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with uintDebugger.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "qtDLGWatch.h"
#include "qtDLGUintDebugger.h"
#include "clsExpressionEvaluator.h"
#include "clsAPIImport.h"

#include <QInputDialog>
#include <QTableWidgetItem>

qtDLGWatch::qtDLGWatch(QWidget *parent)
	: QDockWidget(parent)
{
	setupUi(this);

	tblWatch->horizontalHeader()->resizeSection(0, 150);
	tblWatch->horizontalHeader()->resizeSection(1, 100);
	tblWatch->horizontalHeader()->resizeSection(2, 100);
	tblWatch->horizontalHeader()->resizeSection(3, 40);
	tblWatch->horizontalHeader()->setFixedHeight(21);

	connect(pbAdd,     &QPushButton::clicked, this, &qtDLGWatch::OnAdd);
	connect(pbRemove,  &QPushButton::clicked, this, &qtDLGWatch::OnRemove);
	connect(pbRefresh, &QPushButton::clicked, this, &qtDLGWatch::Refresh);
	connect(tblWatch,  &QTableWidget::cellChanged, this, &qtDLGWatch::OnCellChanged);
}

qtDLGWatch::~qtDLGWatch()
{
}

void qtDLGWatch::AddExpression(const QString &expr, int byteSize, const QString &note)
{
	WatchEntry e;
	e.expression = expr;
	e.byteSize   = byteSize;
	e.note       = note;
	m_entries.append(e);
	RebuildTable();
	Refresh();
}

void qtDLGWatch::Refresh()
{
	// Disconnect cellChanged to avoid re-entrancy while we fill the table.
	disconnect(tblWatch, &QTableWidget::cellChanged, this, &qtDLGWatch::OnCellChanged);

	for(int i = 0; i < m_entries.size(); i++)
	{
		bool ok = false;
		quint64 val = EvaluateEntry(m_entries[i], ok);

		// Apply size mask
		quint64 mask = ~quint64(0);
		if(m_entries[i].byteSize == 1)      mask = 0xFF;
		else if(m_entries[i].byteSize == 2) mask = 0xFFFF;
		else if(m_entries[i].byteSize == 4) mask = 0xFFFFFFFF;
		val &= mask;

		const QString hexStr = ok ? QString("0x%1").arg(val, 0, 16, QChar('0')) : QString("???");
		const QString decStr = ok ? QString::number(val) : QString("???");

		if(tblWatch->item(i, 1)) tblWatch->item(i, 1)->setText(hexStr);
		if(tblWatch->item(i, 2)) tblWatch->item(i, 2)->setText(decStr);
	}

	connect(tblWatch, &QTableWidget::cellChanged, this, &qtDLGWatch::OnCellChanged);
}

void qtDLGWatch::OnAdd()
{
	bool ok = false;
	const QString expr = QInputDialog::getText(this, "Add Watch Expression",
		"Expression (e.g. eax, [esp+4], 0x401000):", QLineEdit::Normal,
		QString(), &ok);
	if(!ok || expr.trimmed().isEmpty())
		return;
	AddExpression(expr.trimmed());
}

void qtDLGWatch::OnRemove()
{
	const int row = tblWatch->currentRow();
	if(row < 0 || row >= m_entries.size())
		return;
	m_entries.removeAt(row);
	RebuildTable();
	Refresh();
}

void qtDLGWatch::OnCellChanged(int row, int col)
{
	if(row < 0 || row >= m_entries.size())
		return;

	QTableWidgetItem *item = tblWatch->item(row, col);
	if(!item)
		return;

	// Allow editing Expression (col 0) and Note (col 4)
	if(col == 0)
	{
		m_entries[row].expression = item->text().trimmed();
		Refresh();
	}
	else if(col == 3)
	{
		const int sz = item->text().toInt();
		if(sz == 1 || sz == 2 || sz == 4 || sz == 8)
			m_entries[row].byteSize = sz;
		Refresh();
	}
	else if(col == 4)
	{
		m_entries[row].note = item->text();
	}
}

void qtDLGWatch::RebuildTable()
{
	disconnect(tblWatch, &QTableWidget::cellChanged, this, &qtDLGWatch::OnCellChanged);

	tblWatch->setRowCount(0);
	for(const WatchEntry &e : m_entries)
	{
		const int row = tblWatch->rowCount();
		tblWatch->insertRow(row);

		tblWatch->setItem(row, 0, new QTableWidgetItem(e.expression));
		tblWatch->setItem(row, 1, new QTableWidgetItem("???"));
		tblWatch->setItem(row, 2, new QTableWidgetItem("???"));

		QTableWidgetItem *sizeItem = new QTableWidgetItem(QString::number(e.byteSize));
		tblWatch->setItem(row, 3, sizeItem);

		tblWatch->setItem(row, 4, new QTableWidgetItem(e.note));
	}

	connect(tblWatch, &QTableWidget::cellChanged, this, &qtDLGWatch::OnCellChanged);
}

quint64 qtDLGWatch::EvaluateEntry(const WatchEntry &e, bool &ok) const
{
	ok = false;

	qtDLGUintDebugger *pMain = qtDLGUintDebugger::GetInstance();
	if(!pMain || !pMain->coreDebugger)
		return 0;

	if(!pMain->coreDebugger->GetDebuggingState())
		return 0;

	HANDLE hProc = pMain->coreDebugger->GetCurrentProcessHandle();
	if(!hProc || hProc == INVALID_HANDLE_VALUE)
		return 0;

#ifdef _AMD64_
	BOOL bIsWOW64 = false;
	if(clsAPIImport::pIsWow64Process)
		clsAPIImport::pIsWow64Process(hProc, &bIsWOW64);

	if(bIsWOW64)
		return clsExpressionEvaluator::evaluate(e.expression, hProc,
			&pMain->coreDebugger->wowProcessContext, true, &ok);
	else
		return clsExpressionEvaluator::evaluate(e.expression, hProc,
			&pMain->coreDebugger->ProcessContext, false, &ok);
#else
	return clsExpressionEvaluator::evaluate(e.expression, hProc,
		&pMain->coreDebugger->ProcessContext, false, &ok);
#endif
}
