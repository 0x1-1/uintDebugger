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
#include "clsProjectFile.h"
#include "clsMemManager.h"

#include <QFileDialog>
#include <QMessageBox>

namespace
{
	QFileDialog::Options ProjectDialogOptions()
	{
		return {};
	}

	bool IsRootElementName(const QStringView &name)
	{
		return name == QStringLiteral("uintDebugger-DATA")
			|| name == QStringLiteral("uintDebugger_DATA");
	}

	bool ReadDebugDataElement(QXmlStreamReader &xmlReader, QString *filePath, QString *commandLine)
	{
		QString localFilePath;
		QString localCommandLine;

		xmlReader.readNext();

		while(!(xmlReader.tokenType() == QXmlStreamReader::EndElement && xmlReader.name() == QStringLiteral("TARGET")))
		{
			if(xmlReader.atEnd() || xmlReader.hasError())
				return false;

			if(xmlReader.tokenType() == QXmlStreamReader::StartElement)
			{
				if(xmlReader.name() == QStringLiteral("FilePath"))
				{
					xmlReader.readNext();
					localFilePath = xmlReader.text().toString();
				}
				else if(xmlReader.name() == QStringLiteral("CommandLine"))
				{
					xmlReader.readNext();
					localCommandLine = xmlReader.text().toString();
				}
			}

			xmlReader.readNext();
		}

		if(localFilePath.isEmpty())
			return false;

		if(filePath != NULL)
			*filePath = localFilePath;
		if(commandLine != NULL)
			*commandLine = localCommandLine;

		return true;
	}
}

clsProjectFile::clsProjectFile(bool isSaveFile, bool *pStartDebugging, QString projectFile, bool bSilent) :
	m_pMainWindow(qtDLGUintDebugger::GetInstance())
{
	if(isSaveFile)
	{
		if(projectFile.length() <= 0)
		{
			projectFile = QFileDialog::getSaveFileName(m_pMainWindow,
				"Please select a save path",
				QDir::currentPath(),
				"uintDebugger Project Files (*.ndb)",
				nullptr,
				ProjectDialogOptions());

			if(projectFile.length() <= 0)
			{
				if(!bSilent)
					QMessageBox::critical(m_pMainWindow, "uintDebugger", "Invalid file selected!", QMessageBox::Ok, QMessageBox::Ok);
				return;
			}
		}

		if(!WriteDataToFile(projectFile))
		{
			if(!bSilent)
				QMessageBox::critical(m_pMainWindow, "uintDebugger", "Error while saving the data!", QMessageBox::Ok, QMessageBox::Ok);
		}
		else
		{
			if(!bSilent)
				QMessageBox::information(m_pMainWindow, "uintDebugger", "Data has been saved!", QMessageBox::Ok, QMessageBox::Ok);
		}
	}
	else
	{
		if(projectFile.length() <= 0)
		{
			projectFile = QFileDialog::getOpenFileName(m_pMainWindow,
				"Please select a file to load",
				QDir::currentPath(),
				"uintDebugger Project Files (*.ndb)",
				nullptr,
				ProjectDialogOptions());

			if(projectFile.length() <= 0)
			{
				if(!bSilent)
					QMessageBox::critical(m_pMainWindow, "uintDebugger", "Invalid file selected!", QMessageBox::Ok, QMessageBox::Ok);
				return;
			}
		}

		if(!ReadDataFromFile(projectFile))
		{
			if(!bSilent)
				QMessageBox::critical(m_pMainWindow, "uintDebugger", "Error while reading the data!", QMessageBox::Ok, QMessageBox::Ok);
			return;
		}
		else
		{
			if(!bSilent)
				QMessageBox::information(m_pMainWindow, "uintDebugger", "Data has been loaded!", QMessageBox::Ok, QMessageBox::Ok);
		}

		if(pStartDebugging != NULL)
			*pStartDebugging = true;

	}
}

clsProjectFile::~clsProjectFile()
{
	m_pMainWindow = NULL;
}

bool clsProjectFile::WriteDataToFile(const QString &saveFilePath)
{
	QFile saveFile(saveFilePath);
	if(!saveFile.open(QIODevice::WriteOnly))
		return false;

	QXmlStreamWriter xmlWriter(&saveFile);
	xmlWriter.setAutoFormatting(true);

	xmlWriter.writeStartDocument();
	xmlWriter.writeStartElement("uintDebugger-DATA");
	xmlWriter.writeAttribute("schemaVersion", QString::number(kNdbSchemaVersion));

	WriteDebugDataToFile(xmlWriter);
	WriteBookmarkDataToFile(xmlWriter);
	WritePatchDataToFile(xmlWriter);
	WriteBreakpointDataToFile(xmlWriter);
	WriteWatchDataToFile(xmlWriter);

	xmlWriter.writeEndElement();
	xmlWriter.writeEndDocument();

	saveFile.close();
	return true;
}

void clsProjectFile::WritePatchDataToFile(QXmlStreamWriter &xmlWriter)
{
	QList<PatchData> tempPatchList = qtDLGPatchManager::GetPatchList();
	QString tempPatchData;

	for(int i = 0; i < tempPatchList.size(); i++)
	{
		xmlWriter.writeStartElement(QString("PATCH_%1").arg(i));
		xmlWriter.writeTextElement("patchOffset",	QString("%1").arg(tempPatchList.at(i).Offset - tempPatchList.at(i).BaseOffset, 16, 16, QChar('0')));
		xmlWriter.writeTextElement("patchSize",		QString("%1").arg(tempPatchList.at(i).PatchSize));
		xmlWriter.writeTextElement("patchModule",	QString::fromWCharArray(tempPatchList.at(i).ModuleName));
		xmlWriter.writeTextElement("patchPMod",		tempPatchList.at(i).processModule);
	
		tempPatchData.clear();
		for(int patchData = 0; patchData < tempPatchList.at(i).PatchSize; patchData++)
			tempPatchData.append(QString("%1").arg(*((BYTE *)tempPatchList.at(i).newData + patchData), 2, 16, QChar('0')));

		xmlWriter.writeTextElement("patchNewData",	tempPatchData);

		tempPatchData.clear();
		for(int patchData = 0; patchData < tempPatchList.at(i).PatchSize; patchData++)
			tempPatchData.append(QString("%1").arg(*((BYTE *)tempPatchList.at(i).orgData + patchData), 2, 16, QChar('0')));

		xmlWriter.writeTextElement("patchOrgData",	tempPatchData);
		xmlWriter.writeEndElement();
	}
}

void clsProjectFile::WriteBookmarkDataToFile(QXmlStreamWriter &xmlWriter)
{
	QList<BookmarkData> tempBookmarkList = qtDLGBookmark::BookmarkGetList();

	for(int i = 0; i < tempBookmarkList.size(); i++)
	{
		xmlWriter.writeStartElement(QString("BOOKMARK_%1").arg(i));
		xmlWriter.writeTextElement("bookmarkOffset",	QString("%1").arg(tempBookmarkList.at(i).bookmarkOffset - tempBookmarkList.at(i).bookmarkBaseOffset, 16, 16, QChar('0')));
		xmlWriter.writeTextElement("bookmarkComment",	tempBookmarkList.at(i).bookmarkComment);
		xmlWriter.writeTextElement("bookmarkModule",	tempBookmarkList.at(i).bookmarkModule);
		xmlWriter.writeTextElement("bookmarkPMod",		tempBookmarkList.at(i).bookmarkProcessModule);
		xmlWriter.writeEndElement();
	}
}

void clsProjectFile::WriteDebugDataToFile(QXmlStreamWriter &xmlWriter)
{
	xmlWriter.writeStartElement("TARGET");
	xmlWriter.writeTextElement("FilePath", m_pMainWindow->coreDebugger->GetTarget());
	xmlWriter.writeTextElement("CommandLine", m_pMainWindow->coreDebugger->GetCMDLine());
	xmlWriter.writeEndElement();
}

void clsProjectFile::WriteBreakpointDataToFile(QXmlStreamWriter &xmlWriter)
{
	WriteBreakpointListToFile(m_pMainWindow->coreBPManager->SoftwareBPs, SOFTWARE_BP, xmlWriter);
	WriteBreakpointListToFile(m_pMainWindow->coreBPManager->MemoryBPs, MEMORY_BP, xmlWriter);
	WriteBreakpointListToFile(m_pMainWindow->coreBPManager->HardwareBPs, HARDWARE_BP, xmlWriter);
}

void clsProjectFile::WriteBreakpointListToFile(QList<BPStruct> &tempBP, int bpType, QXmlStreamWriter &xmlWriter)
{
	QString bpTypeString;

	switch(bpType)
	{
	case SOFTWARE_BP:	bpTypeString = "SW_BP"; break;
	case MEMORY_BP:		bpTypeString = "MEM_BP"; break;
	case HARDWARE_BP:	bpTypeString = "HW_BP"; break;
	default: return;
	}

	for(int i = 0; i < tempBP.size(); i++)
	{
		if(tempBP.at(i).dwHandle == BP_KEEP)
		{
			xmlWriter.writeStartElement(QString("BREAKPOINT_%1_%2").arg(bpTypeString).arg(i));

			xmlWriter.writeTextElement("breakpointOffset",		QString("%1").arg(tempBP.at(i).dwOffset - tempBP.at(i).dwBaseOffset, 16, 16, QChar('0')));
			xmlWriter.writeTextElement("breakpointSize",		QString("%1").arg(tempBP.at(i).dwSize, 8, 16, QChar('0')));
			xmlWriter.writeTextElement("breakpointTypeFlag",	QString("%1").arg(tempBP.at(i).dwTypeFlag, 8, 16, QChar('0')));
			xmlWriter.writeTextElement("breakpointModuleName",	QString(QString::fromWCharArray(tempBP.at(i).moduleName)));

			if(bpType == SOFTWARE_BP)
				xmlWriter.writeTextElement("breakpointDataType", QString("%1").arg(tempBP.at(i).dwDataType, 8, 16, QChar('0')));

			if(tempBP.at(i).dwHitTarget > 0)
				xmlWriter.writeTextElement("breakpointHitTarget", QString::number(tempBP.at(i).dwHitTarget));

			if(tempBP.at(i).dwTID != 0)
				xmlWriter.writeTextElement("breakpointTID", QString::number(tempBP.at(i).dwTID));

			if(tempBP.at(i).comment != NULL)
				xmlWriter.writeTextElement("breakpointComment", QString::fromWCharArray(tempBP.at(i).comment));

			if(!tempBP.at(i).sCondition.isEmpty())
				xmlWriter.writeTextElement("breakpointCondition", tempBP.at(i).sCondition);

			xmlWriter.writeEndElement();
		}
	}
}

bool clsProjectFile::ReadDataFromFile(const QString &loadFilePath)
{
	if(!ValidateDataFromFile(loadFilePath))
		return false;

	QFile loadFile(loadFilePath);
	if(!loadFile.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;

	m_pMainWindow->ClearDebugData(true);

	QXmlStreamReader xmlReader(&loadFile);
	while(!xmlReader.atEnd() && !xmlReader.hasError())
	{
		QXmlStreamReader::TokenType token = xmlReader.readNext();
		if(token == QXmlStreamReader::StartDocument)
		{
			continue;
		}
		else if(token == QXmlStreamReader::StartElement)
		{
			if(IsRootElementName(xmlReader.name()))
			{
				continue;
			}
			else if(xmlReader.name() == "TARGET")
			{
				if(!ReadDebugDataFromFile(xmlReader))
				{
					loadFile.close();
					return false;
				}
			}
			else if(xmlReader.name().contains(u"BOOKMARK_"))
			{
				ReadBookmarkDataFromFile(xmlReader);
			}
			else if(xmlReader.name().contains(u"PATCH_"))
			{
				ReadPatchDataFromFile(xmlReader);
			}
			else if(xmlReader.name().contains(u"BREAKPOINT_"))
			{
				ReadBreakpointDataFromFile(xmlReader);
			}
			else if(xmlReader.name().contains(u"WATCH_"))
			{
				ReadWatchDataFromFile(xmlReader);
			}
		}
	}

	if(xmlReader.hasError())
	{
		loadFile.close();
		return false;
	}

	loadFile.close();
	return true;
}

bool clsProjectFile::ValidateDataFromFile(const QString &loadFilePath, QString *filePath, QString *commandLine)
{
	QFile loadFile(loadFilePath);
	if(!loadFile.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;

	bool foundRoot = false;
	bool foundTarget = false;
	QString parsedFilePath;
	QString parsedCommandLine;

	QXmlStreamReader xmlReader(&loadFile);
	while(!xmlReader.atEnd() && !xmlReader.hasError())
	{
		const QXmlStreamReader::TokenType token = xmlReader.readNext();
		if(token != QXmlStreamReader::StartElement)
			continue;

		if(IsRootElementName(xmlReader.name()))
		{
			int schemaVersion = 0;
			if(!NdbSchema::ReadSchemaVersion(xmlReader.attributes(), &schemaVersion))
			{
				loadFile.close();
				return false;
			}
			foundRoot = true;
			continue;
		}

		if(xmlReader.name() == QStringLiteral("TARGET"))
		{
			if(!ReadDebugDataElement(xmlReader, &parsedFilePath, &parsedCommandLine))
			{
				loadFile.close();
				return false;
			}
			foundTarget = true;
		}
	}

	const bool valid = foundRoot && foundTarget && !parsedFilePath.isEmpty() && !xmlReader.hasError();
	if(valid)
	{
		if(filePath != NULL)
			*filePath = parsedFilePath;
		if(commandLine != NULL)
			*commandLine = parsedCommandLine;
	}

	loadFile.close();
	return valid;
}

bool clsProjectFile::ReadDebugDataFromFile(QXmlStreamReader &xmlReader)
{
	QString filePath, commandLine;

	if(!ReadDebugDataElement(xmlReader, &filePath, &commandLine))
		return false;

	m_pMainWindow->coreDebugger->SetTarget(filePath);
	m_pMainWindow->coreDebugger->SetCommandLine(commandLine);

	return true;
}

void clsProjectFile::ReadBookmarkDataFromFile(QXmlStreamReader &xmlReader)
{
	QString bmOffset, bmComment, bmModule, bmPModule;

	xmlReader.readNext();

	while(!(xmlReader.tokenType() == QXmlStreamReader::EndElement && xmlReader.name().contains(u"BOOKMARK_")))
	{
		if(xmlReader.tokenType() == QXmlStreamReader::StartElement)
		{
			if(xmlReader.name() == "bookmarkOffset")
			{
				xmlReader.readNext();
				bmOffset = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "bookmarkComment")
			{
				xmlReader.readNext();
				bmComment = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "bookmarkModule")
			{
				xmlReader.readNext();
				bmModule = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "bookmarkPMod")
			{
				xmlReader.readNext();
				bmPModule = xmlReader.text().toString();
			}
		}

		xmlReader.readNext();
	}

	if(bmOffset.length() > 0 && bmComment.length() > 0 && bmModule.length() > 0 && bmPModule.length() > 0)
	{
		BookmarkData newBookmark = { 0 };
		newBookmark.bookmarkComment = bmComment;
		newBookmark.bookmarkModule = bmModule;
		newBookmark.bookmarkProcessModule = bmPModule;
		newBookmark.bookmarkOffset = bmOffset.toULongLong(0,16);

		m_pMainWindow->dlgBookmark->BookmarkInsertFromProjectFile(newBookmark);
	}
}

void clsProjectFile::ReadBreakpointDataFromFile(QXmlStreamReader &xmlReader)
{
	QString bpOffset, bpSize, bpTypeFlag, bpModName, bpDataType, bpHitTarget, bpTID, bpComment, bpCondition;
	int bpType = NULL;

	if(xmlReader.name().contains(u"BREAKPOINT_SW_BP"))
		bpType = SOFTWARE_BP;
	else if(xmlReader.name().contains(u"BREAKPOINT_HW_BP"))
		bpType = HARDWARE_BP;
	else if(xmlReader.name().contains(u"BREAKPOINT_MEM_BP"))
		bpType = MEMORY_BP;

	xmlReader.readNext();

	while(!(xmlReader.tokenType() == QXmlStreamReader::EndElement && xmlReader.name().contains(u"BREAKPOINT_")))
	{
		if(xmlReader.tokenType() == QXmlStreamReader::StartElement)
		{
			if(xmlReader.name() == "breakpointOffset")
			{
				xmlReader.readNext();
				bpOffset = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "breakpointSize")
			{
				xmlReader.readNext();
				bpSize = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "breakpointTypeFlag")
			{
				xmlReader.readNext();
				bpTypeFlag = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "breakpointModuleName")
			{
				xmlReader.readNext();
				bpModName = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "breakpointDataType")
			{
				xmlReader.readNext();
				bpDataType = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "breakpointHitTarget")
			{
				xmlReader.readNext();
				bpHitTarget = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "breakpointTID")
			{
				xmlReader.readNext();
				bpTID = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "breakpointComment")
			{
				xmlReader.readNext();
				bpComment = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "breakpointCondition")
			{
				xmlReader.readNext();
				bpCondition = xmlReader.text().toString();
			}
		}

		xmlReader.readNext();
	}

	if(bpOffset.length() > 0 && bpSize.length() > 0 && bpModName.length() > 0 && bpTypeFlag.length() > 0)
	{
		BPStruct newBreakpoint = { 0 };
		newBreakpoint.dwOffset = bpOffset.toULongLong(0, 16);
		newBreakpoint.dwSize = bpSize.toUInt();
		newBreakpoint.dwTypeFlag = bpTypeFlag.toInt(0, 16);

		if(bpType == SOFTWARE_BP && bpDataType.length() > 0)
			newBreakpoint.dwDataType = bpDataType.toInt();

		if(bpHitTarget.length() > 0)
			newBreakpoint.dwHitTarget = bpHitTarget.toUInt();

		if(bpTID.length() > 0)
			newBreakpoint.dwTID = bpTID.toUInt();

		if(bpComment.length() > 0)
		{
			newBreakpoint.comment = (PTCHAR)clsMemManager::CAlloc((bpComment.length() + 1) * sizeof(TCHAR));
			if(newBreakpoint.comment != NULL)
				bpComment.toWCharArray(newBreakpoint.comment);
		}

		if(!bpCondition.isEmpty())
			newBreakpoint.sCondition = bpCondition;

		newBreakpoint.dwHandle = BP_KEEP;
		newBreakpoint.dwPID = -1;
		newBreakpoint.moduleName = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));
		ZeroMemory(newBreakpoint.moduleName, MAX_PATH * sizeof(TCHAR));
		bpModName.toWCharArray(newBreakpoint.moduleName);

		clsBreakpointManager::BreakpointInsertFromProjectFile(newBreakpoint, bpType);
	}
}

void clsProjectFile::ReadPatchDataFromFile(QXmlStreamReader &xmlReader)
{
	QString patchOffset, patchSize, patchModule, patchPMod, patchNewData, patchOrgData;

	xmlReader.readNext();

	while(!(xmlReader.tokenType() == QXmlStreamReader::EndElement && xmlReader.name().contains(u"PATCH_")))
	{
		if(xmlReader.tokenType() == QXmlStreamReader::StartElement)
		{
			if(xmlReader.name() == "patchOffset")
			{
				xmlReader.readNext();
				patchOffset = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "patchSize")
			{
				xmlReader.readNext();
				patchSize = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "patchModule")
			{
				xmlReader.readNext();
				patchModule = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "patchPMod")
			{
				xmlReader.readNext();
				patchPMod = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "patchNewData")
			{
				xmlReader.readNext();
				patchNewData = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "patchOrgData")
			{
				xmlReader.readNext();
				patchOrgData = xmlReader.text().toString();
			}
		}

		xmlReader.readNext();
	}

	if(patchOffset.length() > 0 && patchSize.length() > 0 && patchModule.length() > 0 && patchPMod.length() > 0 && patchNewData.length() > 0 && patchOrgData.length() > 0)
	{
		PatchData newPatch = { 0 };
		newPatch.Offset = patchOffset.toULongLong(0,16);
		newPatch.PatchSize = patchSize.toInt();
		newPatch.processModule = patchPMod;
		newPatch.newData = clsMemManager::CAlloc(newPatch.PatchSize);
		newPatch.orgData = clsMemManager::CAlloc(newPatch.PatchSize);
		newPatch.ModuleName = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));

		ZeroMemory(newPatch.ModuleName, MAX_PATH * sizeof(TCHAR));
		patchModule.toWCharArray(newPatch.ModuleName);

		BYTE tempNewData = NULL, tempOrgData = NULL;
		LPBYTE newData = static_cast<LPBYTE>(newPatch.newData);
		LPBYTE orgData = static_cast<LPBYTE>(newPatch.orgData);
		for(int i = 0, d = 0; i < newPatch.PatchSize; i++ , d += 2)
		{
			tempNewData = patchNewData.mid(d, 2).toInt(0, 16);
			tempOrgData = patchOrgData.mid(d, 2).toInt(0, 16);

			newData[i] = tempNewData;
			orgData[i] = tempOrgData;
		}
		
		qtDLGPatchManager::InsertPatchFromProjectFile(newPatch);
	}
}

void clsProjectFile::WriteWatchDataToFile(QXmlStreamWriter &xmlWriter)
{
	if(!m_pMainWindow->dlgWatch)
		return;

	const QList<qtDLGWatch::WatchEntry> &entries = m_pMainWindow->dlgWatch->entries();
	for(int i = 0; i < entries.size(); i++)
	{
		xmlWriter.writeStartElement(QString("WATCH_%1").arg(i));
		xmlWriter.writeTextElement("watchExpression", entries.at(i).expression);
		xmlWriter.writeTextElement("watchSize",       QString::number(entries.at(i).byteSize));
		if(!entries.at(i).note.isEmpty())
			xmlWriter.writeTextElement("watchNote", entries.at(i).note);
		xmlWriter.writeEndElement();
	}
}

void clsProjectFile::ReadWatchDataFromFile(QXmlStreamReader &xmlReader)
{
	QString wExpr, wNote;
	int wSize = 4;

	xmlReader.readNext();

	while(!(xmlReader.tokenType() == QXmlStreamReader::EndElement && xmlReader.name().contains(u"WATCH_")))
	{
		if(xmlReader.tokenType() == QXmlStreamReader::StartElement)
		{
			if(xmlReader.name() == "watchExpression")
			{
				xmlReader.readNext();
				wExpr = xmlReader.text().toString();
			}
			else if(xmlReader.name() == "watchSize")
			{
				xmlReader.readNext();
				wSize = xmlReader.text().toString().toInt();
			}
			else if(xmlReader.name() == "watchNote")
			{
				xmlReader.readNext();
				wNote = xmlReader.text().toString();
			}
		}
		xmlReader.readNext();
	}

	if(!wExpr.isEmpty() && m_pMainWindow->dlgWatch)
		m_pMainWindow->dlgWatch->AddExpression(wExpr, wSize, wNote);
}
