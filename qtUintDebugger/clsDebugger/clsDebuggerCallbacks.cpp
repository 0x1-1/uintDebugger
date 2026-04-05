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
#include "clsDebugger.h"
#include "clsHelperClass.h"
#include "clsMemManager.h"
#include "dbghelp.h"

bool clsDebugger::PBThreadInfo(DWORD dwPID, DWORD dwTID, quint64 dwEP, bool bSuspended, DWORD dwExitCode, bool isNewThread)
{
	{
		QWriteLocker locker(&m_stateLock);
		if(isNewThread)
		{
			ThreadStruct newTID;
			newTID.bSuspended = bSuspended;
			newTID.dwEP = dwEP;
			newTID.dwTID = dwTID;
			newTID.dwPID = dwPID;
			newTID.dwExitCode = 0;

			TIDs.append(newTID);
		}
		else
		{
			for(int i = 0;i < TIDs.size();i++)
			{
				if(TIDs[i].dwTID == dwTID && TIDs[i].dwPID == dwPID)
				{
					TIDs[i].dwExitCode = dwExitCode;
					break;
				}
			}
		}
	} // lock released before emit to avoid UI-side deadlock on read lock

	emit OnThread(dwPID, dwTID, dwEP, bSuspended, dwExitCode, isNewThread);
	return true;
}

bool clsDebugger::PBProcInfo(DWORD dwPID, PTCHAR sFileName, quint64 dwEP, DWORD dwExitCode, HANDLE hProc, DWORD64 imageBase, bool isNewProc)
{
	bool bEmitNew = false;

	{
		QWriteLocker locker(&m_stateLock);
		if(isNewProc)
		{
			PIDStruct newPID = { 0 };

			newPID.dwPID = dwPID;
			newPID.dwEP = dwEP;
			newPID.sFileName = sFileName;
			newPID.dwExitCode = dwExitCode;
			newPID.hProc = hProc;
			newPID.bRunning = true;
			newPID.imageBase = imageBase;

			if(!m_normalDebugging)
			{
				m_normalDebugging = true;
				newPID.bKernelBP = true;
			}

			PIDs.append(newPID);
			bEmitNew = true;
		}
		else
		{
			for(int i = 0;i < PIDs.size();i++)
			{
				if(PIDs[i].dwPID == dwPID)
				{
					PIDs[i].dwExitCode = dwExitCode;
					PIDs[i].bRunning = false;
					break;
				}
			}
		}
	} // lock released before emit

	if(bEmitNew)
		emit OnPID(dwPID, QString::fromWCharArray(sFileName), dwExitCode, dwEP, true);
	else
		emit OnPID(dwPID, "", dwExitCode, NULL, false);

	return true;
}

bool clsDebugger::PBExceptionInfo(quint64 dwExceptionOffset, quint64 dwExceptionCode, DWORD dwPID, DWORD dwTID)
{
	QString sModName,sFuncName;
	clsHelperClass::LoadSymbolForAddr(sFuncName, sModName, dwExceptionOffset, GetCurrentProcessHandle(dwPID));

	emit OnException(sFuncName, sModName, dwExceptionOffset, dwExceptionCode, dwPID, dwTID);
	return true;
}

bool clsDebugger::PBDLLInfo(PTCHAR sDLLPath, DWORD dwPID, quint64 dwEP, bool bLoaded, DLLStruct *pFoundDLL)
{
	QString emitPath;
	DWORD   emitPID  = 0;
	quint64 emitBase = 0;

	{
		QWriteLocker locker(&m_stateLock);
		if(!bLoaded && pFoundDLL != NULL)
		{
			pFoundDLL->bLoaded = false;
			emitPath = QString::fromWCharArray(pFoundDLL->sPath);
			emitPID  = pFoundDLL->dwPID;
			emitBase = pFoundDLL->dwBaseAdr;
		}
		else
		{
			if(sDLLPath == NULL) return false;

			DLLStruct newDLL;
			newDLL.bLoaded   = true;
			newDLL.dwBaseAdr = dwEP;
			newDLL.sPath     = sDLLPath;
			newDLL.dwPID     = dwPID;

			DLLs.append(newDLL);
			emitPath = QString::fromWCharArray(newDLL.sPath);
			emitPID  = newDLL.dwPID;
			emitBase = newDLL.dwBaseAdr;
		}
	} // lock released before emit

	emit OnDll(emitPath, emitPID, emitBase, bLoaded);
	return true;
}

bool clsDebugger::PBDbgString(PTCHAR sMessage, DWORD dwPID)
{
	emit OnDbgString(QString::fromWCharArray(sMessage), dwPID);

	clsMemManager::CFree(sMessage);
	return true;
}