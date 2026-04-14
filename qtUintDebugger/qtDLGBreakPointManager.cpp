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
#include "qtDLGBreakPointManager.h"
#include "qtDLGUintDebugger.h"

#include "clsHelperClass.h"
#include "clsMemManager.h"

qtDLGBreakPointManager *qtDLGBreakPointManager::pThis = NULL;

qtDLGBreakPointManager::qtDLGBreakPointManager(QWidget *parent, Qt::WindowFlags flags)
	: QWidget(parent, flags)
{
	setupUi(this);
	this->setFixedSize(this->width(),this->height());

	pThis = this;

	cbBreakOn->setEnabled(false);
	cbSize->setEnabled(true);

	// List BP Manager
	tblBPs->horizontalHeader()->resizeSection(0,75);
	tblBPs->horizontalHeader()->resizeSection(1,135);
	tblBPs->horizontalHeader()->resizeSection(2,135);
	tblBPs->horizontalHeader()->resizeSection(3,50);
	tblBPs->horizontalHeader()->resizeSection(4,65); // TID
	tblBPs->horizontalHeader()->resizeSection(5,65); // Hit Target
	tblBPs->horizontalHeader()->resizeSection(6,70); // Break On
	tblBPs->horizontalHeader()->setFixedHeight(21);

	connect(pbClose,SIGNAL(clicked()),this,SLOT(OnClose()));
	connect(pbAddUpdate,SIGNAL(clicked()),this,SLOT(OnAddUpdate()));
	connect(tblBPs,SIGNAL(cellClicked(int,int)),this,SLOT(OnSelectedBPChanged(int,int)));
	connect(tblBPs,SIGNAL(itemDoubleClicked(QTableWidgetItem *)),this,SLOT(OnSendToDisassembler(QTableWidgetItem *)));
	connect(cbType,SIGNAL(currentTextChanged(QString)),this,SLOT(OnBPTypeSelectionChanged(QString)));
	connect(cbOpcode,SIGNAL(currentTextChanged(QString)),this,SLOT(OnBPOpcodeSelectionChanged(QString)));
	connect(new QShortcut(QKeySequence(QKeySequence::Delete),this),SIGNAL(activated()),this,SLOT(OnBPRemove()));
	connect(new QShortcut(Qt::Key_Escape,this),SIGNAL(activated()),this,SLOT(close()));

	m_pAPICompleter = new QCompleter(m_completerList, this);
}

qtDLGBreakPointManager::~qtDLGBreakPointManager()
{

}

void qtDLGBreakPointManager::OnClose()
{
	close();
}

void qtDLGBreakPointManager::OnUpdate(BPStruct newBP,int breakpointType)
{
	if(newBP.dwHandle == BP_KEEP)
	{
		tblBPs->insertRow(tblBPs->rowCount());

		if(newBP.dwPID == -1)
			tblBPs->setItem(tblBPs->rowCount() - 1,0,new QTableWidgetItem(QString("%1").arg(newBP.dwPID)));
		else
			tblBPs->setItem(tblBPs->rowCount() - 1,0,new QTableWidgetItem(QString("%1").arg(newBP.dwPID,0,16)));
		tblBPs->setItem(tblBPs->rowCount() - 1,1,new QTableWidgetItem(QString("%1").arg(newBP.dwOffset,16,16,QChar('0'))));

		QString TempString;
		switch(breakpointType)
		{
		case 0:
			TempString = "Software BP";

			switch(newBP.dwDataType)
			{
				case BP_SW_HLT:     TempString.append(" - HLT"); break;
				case BP_SW_UD2:     TempString.append(" - UD2"); break;
				case BP_SW_LONGINT3:TempString.append(" - LONGINT3"); break;
				default:            TempString.append(" - INT3"); break;
			}

			break;
		case 1:
			TempString = "Memory BP";
			break;
		case 2:
			TempString = "Hardware BP";
			break;
		}
		tblBPs->setItem(tblBPs->rowCount() - 1,2,new QTableWidgetItem(TempString));

		tblBPs->setItem(tblBPs->rowCount() - 1,3,new QTableWidgetItem(QString("%1").arg(newBP.dwSize,2,16,QChar('0'))));

		tblBPs->setItem(tblBPs->rowCount() - 1,4,
			new QTableWidgetItem(newBP.dwTID != 0 ? QString::number(newBP.dwTID, 16) : QString("0")));

		tblBPs->setItem(tblBPs->rowCount() - 1,5,new QTableWidgetItem(QString::number(newBP.dwHitTarget)));

		switch(newBP.dwTypeFlag)
		{
		case BP_EXEC:
			TempString = "Execute";
			break;
		case BP_READ:
			TempString = "Read";
			break;
		case BP_WRITE:
			TempString = "Write";
			break;
		case BP_ACCESS:
			TempString = "Access";
			break;
		}
		tblBPs->setItem(tblBPs->rowCount() - 1,6,new QTableWidgetItem(TempString));

		tblBPs->setItem(tblBPs->rowCount() - 1,7,
			new QTableWidgetItem(newBP.comment != NULL ? QString::fromWCharArray(newBP.comment) : QString()));

		tblBPs->setItem(tblBPs->rowCount() - 1,8,
			new QTableWidgetItem(newBP.sCondition));
	}
	else if(newBP.dwHandle == BP_OFFSETUPDATE)
	{ // BP got new Offset
		for(int i = 0; i < tblBPs->rowCount(); i++)
		{
			if(tblBPs->item(i,1)->text().toULongLong(0,16) == newBP.dwOldOffset)
			{
				tblBPs->removeRow(i);
			}
		}
		newBP.dwHandle = BP_KEEP;

		OnUpdate(newBP,breakpointType);
	}
}

void qtDLGBreakPointManager::OnAddUpdate()
{
	int iUpdateLine = -1;
	quint64 dwOffset = NULL;

	if(leOffset->text().contains("::"))
	{
		QStringList SplitAPIList = leOffset->text().split("::");

		if(SplitAPIList.count() >= 2)
		{
			dwOffset = clsHelperClass::CalcOffsetForModule((PTCHAR)SplitAPIList[0].toLower().toStdWString().c_str(),NULL,lePID->text().toULong(0,16));
			dwOffset = clsHelperClass::RemoteGetProcAddr(SplitAPIList[1],dwOffset,lePID->text().toULong(0,16));

			if(dwOffset <= 0)
			{
				QMessageBox::critical(this,"uintDebugger","Please use a correct API Name!",QMessageBox::Ok,QMessageBox::Ok);
				return;
			}

			leOffset->setText(QString("%1").arg(dwOffset,16,16,QChar('0')));
		}
	}
	else
	{
		dwOffset = leOffset->text().toULongLong(0,16);

		if(dwOffset <= 0)
		{
			QMessageBox::critical(this,"uintDebugger","This offset seems to be invalid!",QMessageBox::Ok,QMessageBox::Ok);
			return;
		}	
	}

	for(int i = 0; i < tblBPs->rowCount(); i++)
	{
		if(QString().compare(tblBPs->item(i,1)->text(),leOffset->text()) == 0)
		{
			iUpdateLine = i;		
			break;
		}
	}

	if(iUpdateLine != -1)
	{
		DWORD dwType = 0;
		if(QString::compare(tblBPs->item(iUpdateLine,2)->text(),"Software BP") == 0)
			dwType = SOFTWARE_BP;
		else if(QString::compare(tblBPs->item(iUpdateLine,2)->text(),"Hardware BP") == 0)
			dwType = HARDWARE_BP;
		else if(QString::compare(tblBPs->item(iUpdateLine,2)->text(),"Memory BP") == 0)
			dwType = MEMORY_BP;

		clsBreakpointManager::BreakpointDelete(tblBPs->item(iUpdateLine,1)->text().toULongLong(0,16),dwType);
		tblBPs->removeRow(iUpdateLine);
	}

	DWORD	dwType = 0,
			dwBreakOn = 0,
			dwBreakpointDataType = 0;

	if(cbBreakOn->currentText().compare("Execute") == 0)
		dwBreakOn = BP_EXEC;
	else if(cbBreakOn->currentText().compare("Read") == 0)
		dwBreakOn = BP_READ;
	else if(cbBreakOn->currentText().compare("Write") == 0)
		dwBreakOn = BP_WRITE;
	else if(cbBreakOn->currentText().compare("Access") == 0)
		dwBreakOn = BP_ACCESS;

	if(cbType->currentText().compare("Software BP") == 0)
	{
		dwType = SOFTWARE_BP;

		if(cbOpcode->currentText().compare("HLT") == 0)
			dwBreakpointDataType = BP_SW_HLT;
		else if(cbOpcode->currentText().compare("UD2") == 0)
			dwBreakpointDataType = BP_SW_UD2;
		else if(cbOpcode->currentText().compare("LONGINT3") == 0)
			dwBreakpointDataType = BP_SW_LONGINT3;
	}
	else if(cbType->currentText().compare("Hardware BP") == 0)
		dwType = HARDWARE_BP;
	else if(cbType->currentText().compare("Memory BP") == 0)
		dwType = MEMORY_BP;


	const DWORD dwHitTarget = (DWORD)sbHitTarget->value();
	const DWORD dwTID       = leTID->text().toUInt(nullptr, 16);

	if(lePID->text().toInt() == -1)
		clsBreakpointManager::BreakpointInsert(dwType, dwBreakOn, lePID->text().toInt(), dwOffset, cbSize->currentText().toInt(), BP_KEEP, dwBreakpointDataType, dwHitTarget, dwTID);
	else
		clsBreakpointManager::BreakpointInsert(dwType, dwBreakOn, lePID->text().toInt(0,16), dwOffset, cbSize->currentText().toInt(), BP_KEEP, dwBreakpointDataType, dwHitTarget, dwTID);

	// Attach comment to the newly-added BP (if one was entered)
	const QString commentText = leComment->text().trimmed();
	if(!commentText.isEmpty())
	{
		clsBreakpointManager *pBPMgr = clsBreakpointManager::GetInstance();
		if(pBPMgr != NULL)
			pBPMgr->SetBPComment(dwOffset, dwType, commentText);
	}

	// Attach condition expression (may be empty — clears any existing condition)
	clsBreakpointManager *pBPMgr = clsBreakpointManager::GetInstance();
	if(pBPMgr != NULL)
		pBPMgr->BreakpointSetCondition(dwOffset, dwType, leCondition->text().trimmed());
}

void qtDLGBreakPointManager::OnSelectedBPChanged(int iRow,int iCol)
{
	lePID->setText(tblBPs->item(iRow,0)->text());
	leOffset->setText(tblBPs->item(iRow,1)->text());

	leTID->setText(tblBPs->item(iRow,4) != nullptr ? tblBPs->item(iRow,4)->text() : QString("0"));

	if(tblBPs->item(iRow,5) != nullptr)
		sbHitTarget->setValue(tblBPs->item(iRow,5)->text().toInt());
	else
		sbHitTarget->setValue(0);

	leComment->setText(tblBPs->item(iRow,7) != nullptr ? tblBPs->item(iRow,7)->text() : QString());
	leCondition->setText(tblBPs->item(iRow,8) != nullptr ? tblBPs->item(iRow,8)->text() : QString());

	DWORD	selectedBreakType	= NULL,
			selectedBPSize		= tblBPs->item(iRow,3)->text().toInt();

	if(QString().compare(tblBPs->item(iRow,6)->text(),"Execute") == 0)
		selectedBreakType = BP_EXEC;
	else if(QString().compare(tblBPs->item(iRow,6)->text(),"Read") == 0)
		selectedBreakType = BP_READ;
	else if(QString().compare(tblBPs->item(iRow,6)->text(),"Write") == 0)
		selectedBreakType = BP_WRITE;
	else if(QString().compare(tblBPs->item(iRow,6)->text(),"Access") == 0)
		selectedBreakType = BP_ACCESS;

	if(tblBPs->item(iRow,2)->text().contains("Software BP"))
	{
		cbBreakOn->clear();
		cbBreakOn->addItem("Execute");
		cbBreakOn->setEnabled(false);

		cbType->setCurrentIndex(0);

		cbSize->clear();
		cbSize->addItem("1");
		cbSize->addItem("2");
		cbSize->addItem("4");
		cbSize->setEnabled(true);

		switch(selectedBPSize)
		{
		case 1: cbSize->setCurrentIndex(0); break;
		case 2: cbSize->setCurrentIndex(1); break;
		case 4: cbSize->setCurrentIndex(2); break;
		}

		cbOpcode->setEnabled(true);

		if(tblBPs->item(iRow,2)->text().contains("LONGINT3"))
			cbOpcode->setCurrentIndex(3);
		else if(tblBPs->item(iRow,2)->text().contains("UD2"))
			cbOpcode->setCurrentIndex(2);
		else if(tblBPs->item(iRow,2)->text().contains("HLT"))
			cbOpcode->setCurrentIndex(1);
		else
			cbOpcode->setCurrentIndex(0);
		
	}
	else if(QString::compare(tblBPs->item(iRow,2)->text(),"Hardware BP") == 0)
	{
		cbBreakOn->clear();
		cbBreakOn->addItem("Execute");
		cbBreakOn->addItem("Read");
		cbBreakOn->addItem("Write");
		cbBreakOn->setEnabled(true);

		cbType->setCurrentIndex(1);

		switch(selectedBreakType)
		{
		case BP_EXEC: cbBreakOn->setCurrentIndex(0); break;
		case BP_READ: cbBreakOn->setCurrentIndex(1); break;
		case BP_WRITE: cbBreakOn->setCurrentIndex(2); break;
		}

		cbSize->clear();
		cbSize->addItem("1");
		cbSize->addItem("2");
		cbSize->addItem("4");
		cbSize->setEnabled(true);

		switch(selectedBPSize)
		{
		case 1: cbSize->setCurrentIndex(0); break;
		case 2: cbSize->setCurrentIndex(1); break;
		case 4: cbSize->setCurrentIndex(2); break;
		}

		cbOpcode->setEnabled(false);
	}
	else if(QString::compare(tblBPs->item(iRow,2)->text(),"Memory BP") == 0)
	{
		cbBreakOn->clear();
		cbBreakOn->addItem("Access");
		cbBreakOn->addItem("Execute");
		cbBreakOn->addItem("Write");
		cbBreakOn->setEnabled(true);

		cbType->setCurrentIndex(2);

		cbSize->clear();
		cbSize->addItem("0");
		cbSize->setEnabled(false);

		switch(selectedBreakType)
		{
		case BP_ACCESS: cbBreakOn->setCurrentIndex(0); break;
		case BP_EXEC: cbBreakOn->setCurrentIndex(1); break;
		case BP_WRITE: cbBreakOn->setCurrentIndex(2); break;
		}	

		cbOpcode->setEnabled(false);
	}
}

void qtDLGBreakPointManager::OnBPRemove()
{
	DWORD dwType = 0;

	for(int i = 0; i < tblBPs->rowCount(); i++)
	{
		if(tblBPs->item(i,0)->isSelected())
		{
			if(tblBPs->item(i,2)->text().compare("Software BP") == 0)
				dwType = SOFTWARE_BP;
			else if(tblBPs->item(i,2)->text().compare("Hardware BP") == 0)
				dwType = HARDWARE_BP;
			else if(tblBPs->item(i,2)->text().compare("Memory BP") == 0)
				dwType = MEMORY_BP;

			clsBreakpointManager::BreakpointDelete(tblBPs->item(i,1)->text().toULongLong(0,16),dwType);
			tblBPs->removeRow(i);
			i = 0;
		}
	}
}

void qtDLGBreakPointManager::UpdateCompleter(QString FilePath,int processID)
{
	QList<APIData> newImports = clsPEManager::getImportsFromFile(FilePath);

	for(int i = 0; i < newImports.size(); i++)
	{
		m_completerList.append(newImports.value(i).APIName);
	}

	delete m_pAPICompleter;
	m_pAPICompleter = new QCompleter(m_completerList, this);

	m_pAPICompleter->setCaseSensitivity(Qt::CaseInsensitive);
	leOffset->setCompleter(m_pAPICompleter);
}

void qtDLGBreakPointManager::DeleteCompleterContent()
{
	m_completerList.clear();
	delete m_pAPICompleter;
	m_pAPICompleter = new QCompleter(m_completerList, this);
}

void qtDLGBreakPointManager::OnDelete(quint64 breakpointOffset)
{
	for(int i = 0; i < tblBPs->rowCount(); i++)
	{
		if(tblBPs->item(i,1)->text().toULongLong(0,16) == breakpointOffset)
		{
			tblBPs->removeRow(i);
			i = 0;
		}
	}
}

QStringList qtDLGBreakPointManager::ReturnCompleterList()
{
	return pThis->m_completerList;
}

void qtDLGBreakPointManager::OnSendToDisassembler(QTableWidgetItem *pItem)
{
	emit OnDisplayDisassembly(tblBPs->item(pItem->row(),1)->text().toULongLong(0,16));
}

void qtDLGBreakPointManager::OnBPTypeSelectionChanged(const QString &selectedItemText)
{
	if(selectedItemText.compare("Software BP") == 0)
	{
		cbBreakOn->clear();
		cbBreakOn->addItem("Execute");
		cbBreakOn->setEnabled(false);

		cbSize->clear();
		cbSize->addItem("1");
		cbSize->addItem("2");
		cbSize->addItem("4");
		cbSize->setEnabled(true);

		cbOpcode->setEnabled(true);
		OnBPOpcodeSelectionChanged(cbOpcode->currentText());
	}
	else if(selectedItemText.compare("Hardware BP") == 0)
	{
		cbBreakOn->clear();
		cbBreakOn->addItem("Execute");
		cbBreakOn->addItem("Read");
		cbBreakOn->addItem("Write");
		cbBreakOn->setEnabled(true);

		cbSize->clear();
		cbSize->addItem("1");
		cbSize->addItem("2");
		cbSize->addItem("4");
		cbSize->setEnabled(true);

		cbOpcode->setEnabled(false);
	}
	else if(selectedItemText.compare("Memory BP") == 0)
	{
		cbBreakOn->clear();
		cbBreakOn->addItem("Access");
		cbBreakOn->addItem("Execute");
		cbBreakOn->addItem("Write");
		cbBreakOn->setEnabled(true);
		
		cbSize->clear();
		cbSize->addItem("0");
		cbSize->setEnabled(false);

		cbOpcode->setEnabled(false);
	}
}

void qtDLGBreakPointManager::OnBPOpcodeSelectionChanged(const QString &selectedItemText)
{
	if(selectedItemText.contains("HLT"))
	{
		cbSize->setEnabled(true);
	}
	else if(selectedItemText.contains("INT3"))
	{
		cbSize->setEnabled(true);
	}
	else if(selectedItemText.contains("UD2"))
	{
		cbSize->setCurrentIndex(1);
		cbSize->setEnabled(false);
	}
	else if(selectedItemText.contains("LONGINT3"))
	{
		cbSize->setCurrentIndex(1);
		cbSize->setEnabled(false);
	}
}
