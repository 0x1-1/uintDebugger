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
#include "qtDLGUintDebugger.h"

#include "clsHelperClass.h"
#include "clsMemManager.h"

#include "dbghelp.h"

#include <Psapi.h>
#include <TlHelp32.h>
#include <QtCore>
#include <ObjIdl.h>
#include <Shobjidl.h>
#include <shlguid.h>

clsHelperClass::clsHelperClass()
{
}

clsHelperClass::~clsHelperClass()
{
}

bool clsHelperClass::LoadSymbolForAddr(QString &functionName, QString &moduleName, quint64 symbolOffset, HANDLE processHandle)
{
	bool bTest = false;
	IMAGEHLP_MODULEW64 imgMod = {0};
	imgMod.SizeOfStruct = sizeof(IMAGEHLP_MODULEW64);
	PSYMBOL_INFOW pSymbol = (PSYMBOL_INFOW)malloc(sizeof(SYMBOL_INFOW) + MAX_PATH * 2);
	memset(pSymbol, 0, sizeof(SYMBOL_INFOW) + MAX_PATH * 2);
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
	pSymbol->MaxNameLen = MAX_PATH;
	quint64 dwDisplacement;

	SymGetModuleInfoW64(processHandle, symbolOffset, &imgMod);
	SymFromAddrW(processHandle, symbolOffset, &dwDisplacement, pSymbol);

	functionName = QString::fromWCharArray(pSymbol->Name);
	moduleName = QString::fromWCharArray(imgMod.ModuleName);

	free(pSymbol);

	return true;
}

bool clsHelperClass::LoadSourceForAddr(QString &fileName, int &lineNumber, quint64 sourceOffset, HANDLE processHandle)
{
	DWORD dwDisplacement = NULL;
	IMAGEHLP_LINEW64 imgSource;
	imgSource.SizeOfStruct = sizeof(imgSource);
	
	if(SymGetLineFromAddrW64(processHandle,sourceOffset,(PDWORD)&dwDisplacement,&imgSource))
	{
		fileName = QString::fromWCharArray(imgSource.FileName);
		lineNumber = imgSource.LineNumber;

		return true;
	}

	return false;
}

PTCHAR clsHelperClass::reverseStrip(PTCHAR lpString, TCHAR lpSearchString)
{
	size_t	iModPos = NULL,
			iModLen = wcslen(lpString);

	if(iModLen > 0)
	{
		for(size_t i = iModLen; i > 0 ; i--)
		{
			if(lpString[i] == lpSearchString)
			{
				iModPos = i;
				break;
			}
		}

		PTCHAR lpTempString = (PTCHAR)clsMemManager::CAlloc((iModLen - iModPos) * sizeof(TCHAR));

		memcpy(lpTempString,(LPVOID)&lpString[iModPos + 1],(iModLen - iModPos) * sizeof(TCHAR));
		return lpTempString;
	}					

	return NULL;
}

QString clsHelperClass::LoadStyleSheet()
{
	return QStringLiteral(R"(
QWidget {
	background-color: #1b1d22;
	color: #e7eaee;
	selection-background-color: #2d7ff9;
	selection-color: #ffffff;
	font-family: "Segoe UI";
	font-size: 9pt;
}

QWidget:disabled {
	color: #7f8793;
}

QMainWindow, QDialog, QDockWidget, QStatusBar, QToolBar {
	background-color: #1b1d22;
	color: #e7eaee;
}

QToolTip {
	background-color: #10141b;
	color: #f7f9fc;
	border: 1px solid #38414d;
	padding: 4px 6px;
}

QMenuBar {
	background-color: #1a1d23;
	color: #f0f3f7;
	border-bottom: 1px solid #2c313a;
}

QMenuBar::item {
	background: transparent;
	padding: 5px 10px;
}

QMenuBar::item:selected {
	background-color: #272c34;
	color: #ffffff;
}

QMenu {
	background-color: #20242c;
	color: #edf1f7;
	border: 1px solid #313845;
	padding: 6px;
}

QMenu::item {
	padding: 6px 22px;
	border-radius: 4px;
}

QMenu::item:selected {
	background-color: #2d7ff9;
	color: #ffffff;
}

QMenu::separator {
	height: 1px;
	background-color: #313845;
	margin: 6px 8px;
}

QToolBar {
	border: none;
	border-bottom: 1px solid #2c313a;
	padding: 4px 6px;
	spacing: 4px;
}

QToolButton, QPushButton {
	background-color: #272c34;
	color: #eef2f7;
	border: 1px solid #38414d;
	border-radius: 4px;
	padding: 4px 10px;
	min-height: 20px;
}

QToolButton:hover, QPushButton:hover {
	background-color: #2f3640;
	border-color: #4b5563;
}

QToolButton:pressed, QPushButton:pressed,
QToolButton:checked, QPushButton:checked,
QPushButton:default {
	background-color: #1f6feb;
	color: #ffffff;
	border-color: #1f6feb;
}

QToolButton:disabled, QPushButton:disabled {
	background-color: #22262d;
	color: #7f8793;
	border-color: #2d333d;
}

QLineEdit, QTextEdit, QPlainTextEdit, QAbstractSpinBox, QComboBox {
	background-color: #15181d;
	color: #edf1f7;
	border: 1px solid #353c46;
	border-radius: 4px;
	padding: 4px 6px;
	selection-background-color: #2d7ff9;
}

QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus,
QAbstractSpinBox:focus, QComboBox:focus {
	border-color: #2d7ff9;
}

QComboBox::drop-down {
	width: 20px;
	border: none;
}

QComboBox QAbstractItemView {
	background-color: #15181d;
	color: #edf1f7;
	border: 1px solid #353c46;
	selection-background-color: #2d7ff9;
}

QTabWidget::pane {
	background-color: #171b22;
	border: 1px solid #313845;
	top: -1px;
}

QTabBar::tab {
	background-color: #232831;
	color: #c5ccd6;
	border: 1px solid #313845;
	padding: 6px 12px;
	margin-right: 2px;
}

QTabBar::tab:selected {
	background-color: #1f6feb;
	color: #ffffff;
	border-color: #1f6feb;
}

QTabBar::tab:hover:!selected {
	background-color: #2b313b;
	color: #edf1f7;
}

QGroupBox {
	border: 1px solid #313845;
	border-radius: 6px;
	margin-top: 14px;
	padding-top: 12px;
}

QGroupBox::title {
	subcontrol-origin: margin;
	left: 12px;
	padding: 0 4px;
	color: #eef2f7;
}

QDockWidget {
	border: 1px solid #313845;
}

QDockWidget::title {
	background-color: #232831;
	color: #eef2f7;
	text-align: left;
	padding: 6px 10px;
	border-bottom: 1px solid #313845;
}

QStatusBar {
	background-color: #171a20;
	color: #c9d0d9;
	border-top: 1px solid #2c313a;
}

QStatusBar::item {
	border: none;
}

QAbstractScrollArea, QListView, QListWidget {
	background-color: #12161c;
	color: #edf1f7;
	border: 1px solid #313845;
}

QTableView, QTableWidget, QTreeView, QTreeWidget, QListView, QListWidget {
	background-color: #12161c;
	alternate-background-color: #181d25;
	color: #edf1f7;
	border: 1px solid #313845;
	gridline-color: #2c313a;
	selection-background-color: #2d7ff9;
	selection-color: #ffffff;
	outline: 0;
	font-family: "Consolas";
	font-size: 9pt;
}

QTableView::item:selected, QTableWidget::item:selected,
QTreeView::item:selected, QTreeWidget::item:selected,
QListView::item:selected, QListWidget::item:selected {
	color: #ffffff;
}

QHeaderView::section {
	background-color: #252a33;
	color: #f4f7fb;
	border: none;
	border-right: 1px solid #343b46;
	border-bottom: 1px solid #343b46;
	padding: 4px 8px;
}

QTableCornerButton::section {
	background-color: #252a33;
	border: none;
	border-right: 1px solid #343b46;
	border-bottom: 1px solid #343b46;
}

QScrollBar:vertical {
	background-color: #12161c;
	width: 12px;
	margin: 0;
}

QScrollBar::handle:vertical {
	background-color: #39414d;
	border-radius: 6px;
	min-height: 24px;
}

QScrollBar::handle:vertical:hover {
	background-color: #4a5563;
}

QScrollBar:horizontal {
	background-color: #12161c;
	height: 12px;
	margin: 0;
}

QScrollBar::handle:horizontal {
	background-color: #39414d;
	border-radius: 6px;
	min-width: 24px;
}

QScrollBar::handle:horizontal:hover {
	background-color: #4a5563;
}

QScrollBar::add-line, QScrollBar::sub-line,
QScrollBar::add-page, QScrollBar::sub-page {
	background: none;
	border: none;
}

QCheckBox, QRadioButton {
	spacing: 8px;
}

QCheckBox::indicator, QRadioButton::indicator {
	width: 14px;
	height: 14px;
}

QCheckBox::indicator:unchecked {
	background-color: #15181d;
	border: 1px solid #48515d;
	border-radius: 3px;
}

QCheckBox::indicator:checked {
	background-color: #1f6feb;
	border: 1px solid #1f6feb;
	border-radius: 3px;
}

QRadioButton::indicator:unchecked {
	background-color: #15181d;
	border: 1px solid #48515d;
	border-radius: 7px;
}

QRadioButton::indicator:checked {
	background-color: #1f6feb;
	border: 1px solid #1f6feb;
	border-radius: 7px;
}

QSplitter::handle {
	background-color: #242931;
}

QLabel#aboutEyebrow {
	color: #8ba3c7;
	font: 600 9pt "Segoe UI";
}

QLabel#aboutTitle {
	color: #f7f9fc;
	font: 600 20pt "Segoe UI";
}

QLabel#aboutTagline {
	color: #ccd6e0;
	font: 10pt "Segoe UI";
}

QFrame#aboutHeroCard {
	background-color: #10141b;
	border: 1px solid #2d3540;
	border-radius: 10px;
}

QFrame#aboutInfoCard, QFrame#aboutSupportCard {
	background-color: #20252d;
	border: 1px solid #313845;
	border-radius: 8px;
}

QLabel#aboutSectionTitle, QLabel#aboutSectionTitle_2 {
	color: #f4f7fb;
	font: 600 10pt "Segoe UI";
}

QLabel#aboutBuildInfo, QLabel#aboutBody, QLabel#aboutFooter {
	color: #c9d2dc;
}

QLabel#aboutFooter {
	font-size: 8pt;
}
)");
}

bool clsHelperClass::IsWindowsXP()
{
	OSVERSIONINFO versionInfo;
	memset (&versionInfo, 0, sizeof(OSVERSIONINFO));
	versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&versionInfo);

	if (versionInfo.dwMinorVersion == 1  &&  versionInfo.dwMajorVersion == 5)
		return true;
	return false;
}

DWORD clsHelperClass::GetMainThread(DWORD ProcessID)
{
	DWORD ThreadID = NULL;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,ProcessID);
    if(hSnap == INVALID_HANDLE_VALUE) return ThreadID;
 
    THREADENTRY32 threadEntry;
    threadEntry.dwSize = sizeof(THREADENTRY32);

	if(!Thread32First(hSnap,&threadEntry))
	{
		CloseHandle(hSnap);
		return ThreadID;
	}

	do
	{
        if(threadEntry.th32OwnerProcessID == ProcessID)
		{
			ThreadID = threadEntry.th32ThreadID;
			break;
		}
	}
	while(Thread32Next(hSnap,&threadEntry));

	CloseHandle(hSnap);
	return ThreadID;
}

quint64 clsHelperClass::CalcOffsetForModule(PTCHAR moduleName,quint64 Offset,DWORD PID)
{
	HANDLE hProc = clsDebugger::GetProcessHandleByPID(PID);
	PTCHAR sTemp = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));
	PTCHAR sTemp2 = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));

	MEMORY_BASIC_INFORMATION mbi;

	quint64 dwAddress = NULL,
			dwBase = NULL;
	
	while(VirtualQueryEx(hProc,(LPVOID)dwAddress,&mbi,sizeof(mbi)))
	{
		// Path
		size_t	iModPos = NULL,
				iModLen = NULL;

		memset(sTemp,0,MAX_PATH * sizeof(TCHAR));
		memset(sTemp2,0,MAX_PATH * sizeof(TCHAR));

		iModLen = GetMappedFileName(hProc,(LPVOID)dwAddress,sTemp2,MAX_PATH);
		if(iModLen > 0)
		{
			for(size_t i = iModLen; i > 0 ; i--)
			{
				if(sTemp2[i] == '\\')
				{
					iModPos = i;
					break;
				}
			}
						
			memcpy_s(sTemp,MAX_PATH,(LPVOID)&sTemp2[iModPos + 1],(iModLen - iModPos) * sizeof(TCHAR));

			if(dwBase == 0)
				dwBase = (DWORD64)mbi.BaseAddress;

			if(wcslen(moduleName) <= 0 && Offset > (DWORD64)mbi.BaseAddress && Offset < ((DWORD64)mbi.BaseAddress + mbi.RegionSize))
			{
				wcscpy_s(moduleName,MAX_PATH,sTemp);
				clsMemManager::CFree(sTemp2);
				clsMemManager::CFree(sTemp);

				return dwBase;
			}
			else if(wcscmp(moduleName,sTemp) == 0)
			{
				clsMemManager::CFree(sTemp2);
				clsMemManager::CFree(sTemp);

				return (DWORD64)mbi.BaseAddress;
			}
		}
		else
			dwBase = 0;

		dwAddress += mbi.RegionSize;
	}

	clsMemManager::CFree(sTemp2);
	clsMemManager::CFree(sTemp);
	return Offset;
}

DWORD64 clsHelperClass::RemoteGetProcAddr(QString apiName, quint64 moduleBase, quint64 processID)
{
	HANDLE processHandle = clsDebugger::GetProcessHandleByPID(processID); // do not close
	IMAGE_DOS_HEADER IDH = {0};
	IMAGE_FILE_HEADER IFH = {0};
	IMAGE_OPTIONAL_HEADER64 IOH64 = {0};
	IMAGE_OPTIONAL_HEADER32 IOH32 = {0};
	IMAGE_EXPORT_DIRECTORY exportTable = {0};
	DWORD exportTableVA = NULL,
		ntSig = NULL;
	bool is64Bit = false;
	
	if(!ReadProcessMemory(processHandle, (LPVOID)moduleBase, &IDH, sizeof(IMAGE_DOS_HEADER), NULL) || IDH.e_magic != IMAGE_DOS_SIGNATURE)
		return 0;

	if(!ReadProcessMemory(processHandle, (LPVOID)(moduleBase + IDH.e_lfanew), &ntSig, sizeof(DWORD), NULL) || ntSig != IMAGE_NT_SIGNATURE)
		return 0;
	
	if(!ReadProcessMemory(processHandle, (LPVOID)(moduleBase + IDH.e_lfanew + sizeof(DWORD)), &IFH, sizeof(IFH), NULL))
		return 0;
 
	if(IFH.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64))
		is64Bit = true;
	else if(IFH.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32))
		is64Bit = false;
	else
		return 0;
 
	if(is64Bit)
	{
		if(!ReadProcessMemory(processHandle, (LPVOID)(moduleBase + IDH.e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)), &IOH64, IFH.SizeOfOptionalHeader, NULL) || IOH64.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
			return 0;

		exportTableVA = IOH64.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	}
	else
	{
		if(!ReadProcessMemory(processHandle, (LPVOID)(moduleBase + IDH.e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)), &IOH32, IFH.SizeOfOptionalHeader, NULL) || IOH32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
			return 0;

		exportTableVA = IOH32.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	}


	if(!ReadProcessMemory(processHandle, (LPVOID)(moduleBase + exportTableVA), &exportTable, sizeof(IMAGE_EXPORT_DIRECTORY), NULL))
		return 0;

	DWORD *ExportFunctionTable = new DWORD[exportTable.NumberOfFunctions];
	DWORD *ExportNameTable = new DWORD[exportTable.NumberOfNames];
	WORD *ExportOrdinalTable = new WORD[exportTable.NumberOfNames];
	
	if(!ReadProcessMemory(processHandle, (LPCVOID)(moduleBase + exportTable.AddressOfNames), ExportNameTable, exportTable.NumberOfNames * sizeof(DWORD), NULL) ||
		!ReadProcessMemory(processHandle, (LPCVOID)(moduleBase + exportTable.AddressOfNameOrdinals), ExportOrdinalTable, exportTable.NumberOfNames * sizeof(WORD), NULL) ||
		!ReadProcessMemory(processHandle, (LPCVOID)(moduleBase + exportTable.AddressOfFunctions), ExportFunctionTable, exportTable.NumberOfFunctions * sizeof(DWORD), NULL))
	{
		delete [] ExportFunctionTable;
		delete [] ExportNameTable;
		delete [] ExportOrdinalTable;

		return 0;
	}

	QString functioName;
	bool isFullString = false;
	CHAR oneCharOfFunction = '\0';

	for(unsigned int i = 0; i < exportTable.NumberOfNames; i++)
	{ 
		isFullString = false;
		functioName.clear(); 

		for(int stringLen = 0; !isFullString; stringLen++)
		{
			if(!ReadProcessMemory(processHandle, (LPCVOID)(moduleBase + ExportNameTable[i] + stringLen), &oneCharOfFunction, sizeof(CHAR), NULL))
			{
				delete [] ExportFunctionTable;
				delete [] ExportNameTable;
				delete [] ExportOrdinalTable;

				return 0;
			}
 
			functioName.append(oneCharOfFunction);

			if(oneCharOfFunction == (CHAR)'\0')
				isFullString = true;
		}
 
		if(functioName.contains(apiName))
		{		
			DWORD64 returnValue = moduleBase + ExportFunctionTable[ExportOrdinalTable[i]];

			delete[] ExportFunctionTable;
			delete[] ExportNameTable;
			delete[] ExportOrdinalTable;

			return returnValue;
		}
	}

	delete [] ExportFunctionTable;
	delete [] ExportNameTable;
	delete [] ExportOrdinalTable;

	return 0;
}

QString clsHelperClass::ResolveShortcut(QString shortcutFile)
{
	HRESULT resultCode = E_FAIL;
	IShellLink* shellLink = nullptr;
	TCHAR resolvedFilePath[MAX_PATH] = {};
	WIN32_FIND_DATA findDataStruct;

	resultCode = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&shellLink);
	if (SUCCEEDED(resultCode))
	{
		IPersistFile* persistFile = nullptr;
		shellLink->QueryInterface(IID_IPersistFile, (void**)&persistFile);
		if (persistFile)
		{
			resultCode = persistFile->Load(shortcutFile.toStdWString().c_str(), STGM_READ);
			if (SUCCEEDED(resultCode))
			{
				resultCode = shellLink->Resolve(NULL, SLR_UPDATE);
				if (SUCCEEDED(resultCode))
					shellLink->GetPath(resolvedFilePath, MAX_PATH, &findDataStruct, SLGP_RAWPATH);
			}
			persistFile->Release();
		}
		shellLink->Release();
	}

	return QString::fromWCharArray(resolvedFilePath);
}

PTCHAR clsHelperClass::GetFileNameFromModuleBase(HANDLE processHandle, LPVOID imageBase) 
{
	PTCHAR tcFilename = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));
	if (!GetMappedFileName(processHandle, imageBase, tcFilename, MAX_PATH)) 
	{
		clsMemManager::CFree(tcFilename);
		return NULL;
	}

	PTCHAR tcTemp = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));
	if (!GetLogicalDriveStrings(MAX_PATH - 1, tcTemp)) 
	{
		clsMemManager::CFree(tcFilename);
		clsMemManager::CFree(tcTemp);
		return NULL;
	}

	PTCHAR tcName = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));
	PTCHAR tcFile = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));
	TCHAR tcDrive[3] = TEXT(" :");
	BOOL bFound = false;
	PTCHAR p = tcTemp;

	do 
	{
		*tcDrive = *p;

		if(QueryDosDevice(tcDrive, tcName, MAX_PATH))
		{
			size_t uNameLen = wcslen(tcName);
			if(uNameLen < MAX_PATH) 
			{
				bFound = _wcsnicmp(tcFilename, tcName, uNameLen) == 0;
				if(bFound)
					swprintf_s(tcFile, MAX_PATH, L"%s%s", tcDrive, (tcFilename + uNameLen));
			}
		}

		while (*p++);
	} while (!bFound && *p);

	clsMemManager::CFree(tcName);
	clsMemManager::CFree(tcTemp);
	clsMemManager::CFree(tcFilename);

	return tcFile;
}
