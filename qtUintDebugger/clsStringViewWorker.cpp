/*
 * 	  This file is part of uintDebugger.
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
#include "clsStringViewWorker.h"
#include "clsMemManager.h"
#include "clsPEManager.h"
#include "clsHelperClass.h"

clsStringViewWorker::clsStringViewWorker(QList<StringProcessingData> dataForProcessing)
{
	setObjectName(QStringLiteral("clsStringViewWorker"));
	m_processingData = dataForProcessing;
	this->start();
}

clsStringViewWorker::~clsStringViewWorker()
{
	if(isRunning())
	{
		requestInterruption();
		wait();
	}

	stringList.clear();
}

void clsStringViewWorker::run()
{
	stringList.clear();
	clsPEManager *pPEManager = clsPEManager::GetInstance();

	for(int i = 0; i < m_processingData.size(); i++)
	{
		if(isInterruptionRequested())
			return;

		const StringProcessingData &entry = m_processingData.at(i);
		if(entry.processHandle == NULL || entry.processHandle == INVALID_HANDLE_VALUE)
			continue;

		PTCHAR moduleName = clsHelperClass::reverseStrip(entry.filePath, TEXT('\\'));
		if(moduleName == NULL)
			continue;

		DWORD64 fileImageBase = clsHelperClass::CalcOffsetForModule(moduleName, NULL, entry.processID);
		clsMemManager::CFree(moduleName);

		if(fileImageBase == 0)
			continue;

		const QString filePath = QString::fromWCharArray(entry.filePath);
		QList<IMAGE_SECTION_HEADER> sections = pPEManager->getSections(filePath, entry.processID);

		for(int s = 0; s < sections.size(); s++)
		{
			if(isInterruptionRequested())
				return;

			const DWORD sectionSize = sections.at(s).SizeOfRawData > 0
				? sections.at(s).SizeOfRawData
				: sections.at(s).Misc.VirtualSize;

			if(sectionSize == 0)
				continue;

			const DWORD64 virtualAddr = (DWORD64)sections.at(s).VirtualAddress + fileImageBase;

			LPVOID sectionBuffer = clsMemManager::CAlloc(sectionSize);
			if(sectionBuffer == NULL)
				continue;

			SIZE_T bytesRead = 0;
			if(ReadProcessMemory(entry.processHandle, (LPVOID)virtualAddr, sectionBuffer, sectionSize, &bytesRead) && bytesRead > 0)
			{
				ParseMemoryForAsciiStrings(virtualAddr, sectionBuffer, (DWORD)bytesRead, entry.processID);
				ParseMemoryForUnicodeStrings(virtualAddr, sectionBuffer, (DWORD)bytesRead, entry.processID);
			}

			clsMemManager::CFree(sectionBuffer);
		}
	}
}

void clsStringViewWorker::ParseMemoryForAsciiStrings(DWORD64 virtualAddress, LPVOID sectionBuffer, DWORD sectionSize, int pid)
{
	const char *buf = reinterpret_cast<const char *>(sectionBuffer);
	QString current;
	DWORD64 stringStart = virtualAddress;

	for(DWORD i = 0; i < sectionSize; i++)
	{
		const char c = buf[i];
		const bool printable = (c >= 0x20 && c <= 0x7E) || c == 0x09 || c == 0x0A || c == 0x0D;

		if(printable)
		{
			if(current.isEmpty())
				stringStart = virtualAddress + i;
			current.append(c);
		}
		else
		{
			if(current.length() >= 4)
				stringList.append(StringData(stringStart, pid, current));
			current.clear();
		}
	}

	if(current.length() >= 4)
		stringList.append(StringData(stringStart, pid, current));
}

void clsStringViewWorker::ParseMemoryForUnicodeStrings(DWORD64 virtualAddress, LPVOID sectionBuffer, DWORD sectionSize, int pid)
{
	if(sectionSize < 2)
		return;

	const wchar_t *buf = reinterpret_cast<const wchar_t *>(sectionBuffer);
	const DWORD wcount = sectionSize / sizeof(wchar_t);
	QString current;
	DWORD64 stringStart = virtualAddress;

	for(DWORD i = 0; i < wcount; i++)
	{
		const wchar_t wc = buf[i];
		const bool printable = (wc >= 0x0020 && wc <= 0x007E) || wc == 0x0009 || wc == 0x000A;

		if(printable)
		{
			if(current.isEmpty())
				stringStart = virtualAddress + (DWORD64)i * sizeof(wchar_t);
			current.append(QChar(wc));
		}
		else
		{
			if(current.length() >= 4)
				stringList.append(StringData(stringStart, pid, current));
			current.clear();
		}
	}

	if(current.length() >= 4)
		stringList.append(StringData(stringStart, pid, current));
}
