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
#include "qtDLGStringView.h"

#include "clsMemManager.h"
#include "clsClipboardHelper.h"

#include <QMenu>

#include "uiHelpers.h"

qtDLGStringView::qtDLGStringView(QWidget *parent, Qt::WindowFlags flags, qint32 processID)
	: QWidget(parent, flags),
	m_processID(processID),
	m_isFinished(false),
	m_pStringProcessor(NULL)
{
	setupUi(this);
	this->setAttribute(Qt::WA_DeleteOnClose,true);
	this->setLayout(horizontalLayout);
	SetPlaceholderRow(tblStringView, QStringLiteral("Scanning strings..."));
	
	// Init List
	tblStringView->horizontalHeader()->resizeSection(0,75);
	tblStringView->horizontalHeader()->resizeSection(1,135);
	tblStringView->horizontalHeader()->setFixedHeight(21);

	// Display
	m_pMainWindow = qtDLGUintDebugger::GetInstance();
	m_maxRows = (tblStringView->verticalHeader()->height() / 15) - 1;

	m_forEntry = 0;
	{
		QReadLocker locker(&m_pMainWindow->coreDebugger->m_stateLock);
		m_forEnd = m_pMainWindow->coreDebugger->PIDs.size();
		for(int i = 0; i < m_pMainWindow->coreDebugger->PIDs.size(); i++)
		{
			if(m_pMainWindow->coreDebugger->PIDs[i].dwPID == m_processID)
			{
				m_forEntry = i;
				m_forEnd = i + 1;
				break;
			}
		}
	}
	
	connect(stringScroll,&QScrollBar::valueChanged,this,&qtDLGStringView::InsertDataFrom);
	connect(tblStringView,&QWidget::customContextMenuRequested,this,&qtDLGStringView::OnCustomContextMenuRequested);
	connect(new QShortcut(QKeySequence("F5"),this),&QShortcut::activated,this,&qtDLGStringView::DataProcessing);
	connect(new QShortcut(Qt::Key_Escape,this),&QShortcut::activated,this,&qtDLGStringView::close);

	DataProcessing();
}

qtDLGStringView::~qtDLGStringView()
{
	if(m_pStringProcessor != NULL)
	{
		m_pStringProcessor->disconnect(this);
		delete m_pStringProcessor;
		m_pStringProcessor = NULL;
	}
}

void qtDLGStringView::DataProcessing()
{
	QList<StringProcessingData> dataForProcessing;
	StringProcessingData newData;
	
	{
		QReadLocker locker(&m_pMainWindow->coreDebugger->m_stateLock);
		for(int i = m_forEntry; i < m_forEnd; i++)
		{
			if(i >= m_pMainWindow->coreDebugger->PIDs.size()) break;
			newData.filePath      = m_pMainWindow->coreDebugger->PIDs[i].sFileName;
			newData.processHandle = m_pMainWindow->coreDebugger->PIDs[i].hProc;
			newData.processID     = m_pMainWindow->coreDebugger->PIDs[i].dwPID;
			dataForProcessing.append(newData);
		}
	}

	if(m_pStringProcessor != NULL)
	{
		m_pStringProcessor->disconnect(this);
		delete m_pStringProcessor;
	}
	m_pStringProcessor = new clsStringViewWorker(dataForProcessing);
	connect(m_pStringProcessor,&QThread::finished,this,&qtDLGStringView::DisplayStrings,Qt::QueuedConnection);
}

void qtDLGStringView::DisplayStrings()
{
	if(m_pStringProcessor->stringList.count() <= 0)
	{
		SetPlaceholderRow(tblStringView, QStringLiteral("No strings were discovered for the selected process."));
		return;
	}

	m_isFinished = true;

	m_maxRows = (tblStringView->verticalHeader()->height() / 15) - 1;

	stringScroll->setValue(0);
	stringScroll->setMaximum(m_pStringProcessor->stringList.count() - (tblStringView->verticalHeader()->height() / 15) + 1);

	InsertDataFrom(0);
}

void qtDLGStringView::InsertDataFrom(int position)
{
	if(position < 0 || !m_isFinished) return;

	if((tblStringView->rowCount() - 1) != m_maxRows)
	{
		int count = 0;
		tblStringView->setRowCount(0);
		
		while(count <= m_maxRows)
		{
			tblStringView->insertRow(0);
			count++;
		}
	}

	int numberOfLines = 0;
	StringData currentStringData;
	while(numberOfLines <= m_maxRows)
	{
		if(position >= m_pStringProcessor->stringList.count())
			break;
		else
		{
			currentStringData = m_pStringProcessor->stringList.at(position);

			// PID
			tblStringView->setItem(numberOfLines,0,
				new QTableWidgetItem(QString::asprintf("%08X",currentStringData.PID)));

			// Offset
			if(currentStringData.StringOffset > 0)
				tblStringView->setItem(numberOfLines,1,
				new QTableWidgetItem(QString("%1").arg(currentStringData.StringOffset,8,16,QChar('0'))));
			else 
				tblStringView->setItem(numberOfLines,1,
				new QTableWidgetItem(""));

			// String
			tblStringView->setItem(numberOfLines,2,
				new QTableWidgetItem(currentStringData.DataString));

			position++;numberOfLines++;
		}
	}
}

void qtDLGStringView::MenuCallback(QAction* pAction)
{
	if(QString().compare(pAction->text(),"Line") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblStringView, -1));
	}
	else if(QString().compare(pAction->text(),"Offset") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblStringView, 1));
	}
	else if(QString().compare(pAction->text(),"String") == 0)
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(clsClipboardHelper::getTableToClipboard(tblStringView, 2));
	}
}

void qtDLGStringView::OnCustomContextMenuRequested(QPoint qPoint)
{
	QMenu menu;

	m_selectedRow = tblStringView->indexAt(qPoint).row();
	if(m_selectedRow < 0) return;

	QMenu *submenu = menu.addMenu("Copy to Clipboard");
	submenu->addAction(new QAction("Line",this));
	submenu->addAction(new QAction("Offset",this));
	submenu->addAction(new QAction("String",this));

	menu.addMenu(submenu);
	connect(&menu,&QMenu::triggered,this,&qtDLGStringView::MenuCallback);

	menu.exec(QCursor::pos());
}

void qtDLGStringView::resizeEvent(QResizeEvent *event)
{
	m_maxRows = (tblStringView->verticalHeader()->height() / 15) - 1;
	stringScroll->setMaximum(m_pStringProcessor->stringList.count() - (tblStringView->verticalHeader()->height() / 15));

	InsertDataFrom(stringScroll->value());
}

void qtDLGStringView::wheelEvent(QWheelEvent *event)
{
	QWheelEvent *pWheel = (QWheelEvent*)event;

	if(pWheel->angleDelta().y() > 0)
	{
		stringScroll->setValue(stringScroll->value() - 1);
	}
	else
	{
		stringScroll->setValue(stringScroll->value() + 1);
	}

	InsertDataFrom(stringScroll->value());
}
