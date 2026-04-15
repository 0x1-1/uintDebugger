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
#include "qtDLGHeapView.h"
#include "qtDLGHexView.h"

#include "clsMemManager.h"
#include "clsMemDump.h"
#include "clsClipboardHelper.h"

#include <TlHelp32.h>

#include <QCoreApplication>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTimer>

namespace
{
QString HeapBlockFlagsToText(quint32 flags)
{
	switch(flags)
	{
	case LF32_FIXED:
		return QStringLiteral("LF32_FIXED");
	case LF32_FREE:
		return QStringLiteral("LF32_FREE");
	case LF32_MOVEABLE:
		return QStringLiteral("LF32_MOVEABLE");
	default:
		return QString("%1").arg(flags, 8, 16, QChar('0'));
	}
}
}

qtDLGHeapView::qtDLGHeapView(QWidget *parent, Qt::WindowFlags flags,int processID)
	: QWidget(parent, flags),
	m_processID(processID),
	m_processCountEntry(0),
	m_processCountEnd(0),
	m_selectedRow(-1),
	m_isRefreshing(false),
	m_pMainWindow(NULL)
{
	setupUi(this);
	this->setAttribute(Qt::WA_DeleteOnClose,true);
	this->setLayout(verticalLayout);

	connect(tblHeapBlocks,&QWidget::customContextMenuRequested,this,&qtDLGHeapView::OnCustomContextMenuRequested);
	connect(tblHeapView,&QTableWidget::itemSelectionChanged,this,&qtDLGHeapView::OnSelectionChanged);
	connect(new QShortcut(QKeySequence("F5"),this),&QShortcut::activated,this,&qtDLGHeapView::DisplayHeap);
	connect(new QShortcut(Qt::Key_Escape,this),&QShortcut::activated,this,&qtDLGHeapView::close);

	tblHeapView->horizontalHeader()->resizeSection(0,75);
	tblHeapView->horizontalHeader()->resizeSection(1,150);
	tblHeapView->horizontalHeader()->resizeSection(2,135);
	tblHeapView->horizontalHeader()->resizeSection(3,135);
	tblHeapView->horizontalHeader()->resizeSection(4,110);
	tblHeapView->horizontalHeader()->setFixedHeight(21);
	tblHeapBlocks->horizontalHeader()->setFixedHeight(21);

	m_pMainWindow = qtDLGUintDebugger::GetInstance();

	{
		QReadLocker locker(&m_pMainWindow->coreDebugger->m_stateLock);
		m_processCountEnd = m_pMainWindow->coreDebugger->PIDs.size();
		for(int i = 0; i < m_pMainWindow->coreDebugger->PIDs.size(); i++)
		{
			if(m_pMainWindow->coreDebugger->PIDs[i].dwPID == m_processID)
			{
				m_processCountEntry = i;
				m_processCountEnd = i + 1;
				break;
			}
		}
	}

	SetTablePlaceholder(tblHeapView, QStringLiteral("Loading heap data..."));
	SetTablePlaceholder(tblHeapBlocks, QStringLiteral("Select a heap to inspect its blocks."));
	QTimer::singleShot(0, this, SLOT(DisplayHeap()));
}

qtDLGHeapView::~qtDLGHeapView()
{
}

void qtDLGHeapView::OnCustomContextMenuRequested(QPoint qPoint)
{
	QMenu menu;

	m_selectedRow = tblHeapBlocks->indexAt(qPoint).row();
	if(m_selectedRow < 0) return;

	menu.addAction(new QAction("Send Offset To HexView",this));
	menu.addAction(new QAction("Dump Memory To File",this));
	menu.addSeparator();
	QMenu *submenu = menu.addMenu("Copy to Clipboard");
	submenu->addAction(new QAction("Line",this));
	submenu->addAction(new QAction("HeapID",this));
	submenu->addAction(new QAction("Address",this));
	submenu->addAction(new QAction("Block Size",this));
	submenu->addAction(new QAction("Block Count",this));
	submenu->addAction(new QAction("Flags",this));

	menu.addMenu(submenu);
	connect(&menu,&QMenu::triggered,this,&qtDLGHeapView::MenuCallback);

	menu.exec(QCursor::pos());
}

void qtDLGHeapView::MenuCallback(QAction* pAction)
{
	if(QString().compare(pAction->text(),"Send Offset To HexView") == 0)
	{
		qtDLGHexView *newView = new qtDLGHexView(this,Qt::Window,tblHeapBlocks->item(m_selectedRow,0)->text().toULongLong(0,16),
			tblHeapBlocks->item(m_selectedRow,2)->text().toULongLong(0,16),
			tblHeapBlocks->item(m_selectedRow,3)->text().toULongLong(0,16));
		newView->show();
	}
	else if(QString().compare(pAction->text(),"Dump Memory To File") == 0)
	{
		HANDLE hProc = clsDebugger::GetProcessHandleByPID(tblHeapBlocks->item(m_selectedRow,0)->text().toULongLong(0,16));

		clsMemDump memDump(hProc,
			(PTCHAR)L"Heap",
			tblHeapBlocks->item(m_selectedRow,2)->text().toULongLong(0,16),
			tblHeapBlocks->item(m_selectedRow,3)->text().toULongLong(0,16),
			this);
	}
	else if(QString().compare(pAction->text(),"Line") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblHeapBlocks, -1));
	}
	else if(QString().compare(pAction->text(),"HeapID") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblHeapBlocks, 1));
	}
	else if(QString().compare(pAction->text(),"Address") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblHeapBlocks, 2));
	}
	else if(QString().compare(pAction->text(),"Block Size") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblHeapBlocks, 3));
	}
	else if(QString().compare(pAction->text(),"Block Count") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblHeapBlocks, 4));
	}
	else if(QString().compare(pAction->text(),"Flags") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblHeapBlocks, 5));
	}
}

void qtDLGHeapView::OnSelectionChanged()
{
	if(m_isRefreshing || tblHeapView->rowCount() <= 0)
		return;

	const int selectedRow = tblHeapView->currentRow();
	if(selectedRow < 0 || tblHeapView->item(selectedRow, 0) == NULL || tblHeapView->item(selectedRow, 1) == NULL)
	{
		SetTablePlaceholder(tblHeapBlocks, QStringLiteral("Select a heap to inspect its blocks."));
		return;
	}

	const quint64 processID = tblHeapView->item(selectedRow, 0)->text().toULongLong(0,16);
	const quint64 heapID = tblHeapView->item(selectedRow, 1)->text().toULongLong(0,16);
	LoadHeapBlocks(processID, heapID);
}

void qtDLGHeapView::DisplayHeap()
{
	if(m_isRefreshing)
		return;

	m_isRefreshing = true;
	QApplication::setOverrideCursor(Qt::WaitCursor);

	{
		QSignalBlocker blocker(tblHeapView);
		tblHeapView->clearSpans();
		tblHeapView->setRowCount(0);
	}
	{
		QSignalBlocker blocker(tblHeapBlocks);
		tblHeapBlocks->clearSpans();
		tblHeapBlocks->setRowCount(0);
	}

	for(int i = m_processCountEntry; i < m_processCountEnd; i++)
	{
		DWORD processID;
		{
			QReadLocker locker(&m_pMainWindow->coreDebugger->m_stateLock);
			if(i >= m_pMainWindow->coreDebugger->PIDs.size()) break;
			processID = m_pMainWindow->coreDebugger->PIDs[i].dwPID;
		}
		HEAPLIST32 heapList = { 0 };
		heapList.dwSize = sizeof(HEAPLIST32);
		HANDLE hHeapSnap = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, processID);

		if(hHeapSnap == INVALID_HANDLE_VALUE)
			continue;

		if(Heap32ListFirst(hHeapSnap, &heapList))
		{
			do
			{
				quint64 committedSize = 0;
				quint64 usedSize = 0;
				quint64 blockCount = 0;

				HEAPENTRY32 he = { 0 };
				he.dwSize = sizeof(HEAPENTRY32);

				if(Heap32First(&he, processID, heapList.th32HeapID))
				{
					do
					{
						committedSize += he.dwBlockSize;
						if((he.dwFlags & LF32_FREE) == 0)
							usedSize += he.dwBlockSize;
						blockCount++;

						if((blockCount % 256) == 0)
							QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

						he.dwSize = sizeof(HEAPENTRY32);
					}while(Heap32Next(&he));
				}

				tblHeapView->insertRow(tblHeapView->rowCount());
				tblHeapView->setItem(tblHeapView->rowCount() - 1,0,
					new QTableWidgetItem(QString("%1").arg(heapList.th32ProcessID, 8, 16, QChar('0'))));
				tblHeapView->setItem(tblHeapView->rowCount() - 1,1,
					new QTableWidgetItem(QString("%1").arg(heapList.th32HeapID, 16, 16, QChar('0'))));
				tblHeapView->setItem(tblHeapView->rowCount() - 1,2,
					new QTableWidgetItem(QString::number(usedSize)));
				tblHeapView->setItem(tblHeapView->rowCount() - 1,3,
					new QTableWidgetItem(QString::number(committedSize)));
				tblHeapView->setItem(tblHeapView->rowCount() - 1,4,
					new QTableWidgetItem(QString::number(blockCount)));
				tblHeapView->setItem(tblHeapView->rowCount() - 1,5,
					new QTableWidgetItem(QString("%1").arg(heapList.dwFlags, 8, 16, QChar('0'))));

				QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				heapList.dwSize = sizeof(HEAPLIST32);
			}while(Heap32ListNext(hHeapSnap, &heapList));
		}

		CloseHandle(hHeapSnap);
	}

	if(tblHeapView->rowCount() <= 0)
	{
		SetTablePlaceholder(tblHeapView, QStringLiteral("No heap data available for the selected process."));
		SetTablePlaceholder(tblHeapBlocks, QStringLiteral("No heap blocks available."));
		m_isRefreshing = false;
		QApplication::restoreOverrideCursor();
		return;
	}

	tblHeapView->setCurrentCell(0, 0);
	LoadHeapBlocks(tblHeapView->item(0, 0)->text().toULongLong(0,16),
		tblHeapView->item(0, 1)->text().toULongLong(0,16));

	m_isRefreshing = false;
	QApplication::restoreOverrideCursor();
}

void qtDLGHeapView::LoadHeapBlocks(quint64 processID, quint64 heapID)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);

	{
		QSignalBlocker blocker(tblHeapBlocks);
		tblHeapBlocks->clearSpans();
		tblHeapBlocks->setRowCount(0);
	}

	HEAPENTRY32 he = { 0 };
	he.dwSize = sizeof(HEAPENTRY32);
	quint64 blockCount = 0;

	if(!Heap32First(&he, processID, heapID))
	{
		SetTablePlaceholder(tblHeapBlocks, QStringLiteral("No heap blocks available for the selected heap."));
		QApplication::restoreOverrideCursor();
		return;
	}

	do
	{
		tblHeapBlocks->insertRow(tblHeapBlocks->rowCount());
		tblHeapBlocks->setItem(tblHeapBlocks->rowCount() - 1,0,
			new QTableWidgetItem(QString("%1").arg(processID, 8, 16, QChar('0'))));
		tblHeapBlocks->setItem(tblHeapBlocks->rowCount() - 1,1,
			new QTableWidgetItem(QString("%1").arg(he.th32HeapID, 16, 16, QChar('0'))));
		tblHeapBlocks->setItem(tblHeapBlocks->rowCount() - 1,2,
			new QTableWidgetItem(QString("%1").arg(he.dwAddress, 16, 16, QChar('0'))));
		tblHeapBlocks->setItem(tblHeapBlocks->rowCount() - 1,3,
			new QTableWidgetItem(QString("%1").arg(he.dwBlockSize, 16, 16, QChar('0'))));
		tblHeapBlocks->setItem(tblHeapBlocks->rowCount() - 1,4,
			new QTableWidgetItem(QString::number(++blockCount)));
		tblHeapBlocks->setItem(tblHeapBlocks->rowCount() - 1,5,
			new QTableWidgetItem(HeapBlockFlagsToText(he.dwFlags)));

		if((blockCount % 256) == 0)
			QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		he.dwSize = sizeof(HEAPENTRY32);
	}while(Heap32Next(&he));

	QApplication::restoreOverrideCursor();
}

void qtDLGHeapView::SetTablePlaceholder(QTableWidget *table, const QString &message)
{
	QSignalBlocker blocker(table);

	table->clearSpans();
	table->clearContents();
	table->setRowCount(1);

	QTableWidgetItem *placeholderItem = new QTableWidgetItem(message);
	placeholderItem->setFlags(Qt::ItemIsEnabled);
	placeholderItem->setTextAlignment(Qt::AlignCenter);

	table->setItem(0, 0, placeholderItem);
	table->setSpan(0, 0, 1, table->columnCount());
}
