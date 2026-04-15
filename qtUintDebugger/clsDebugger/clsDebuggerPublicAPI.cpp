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
#include "qtDLGTrace.h"

#include "clsDebugger.h"
#include "clsMemManager.h"
#include "clsAPIImport.h"
#include "clsHelperClass.h"

#include "dbghelp.h"

using namespace std;

bool clsDebugger::SignalDebugEvent()
{
	return SetEvent(m_debugEvent) != 0;
}

bool clsDebugger::DetachFromProcess()
{
	m_normalDebugging = true;
	//_isDebugging = false;
	m_stopDebugging = true;

	m_pBreakpointManager->BreakpointClear();

	for(int d = 0; d < PIDs.size(); d++)
	{
		if(!CheckProcessState(PIDs[d].dwPID))
			break;
		DebugBreakProcess(PIDs[d].hProc);
		SetEvent(m_debugEvent);
	}
	
	return true;
}

bool clsDebugger::AttachToProcess(DWORD dwPID)
{
	CleanWorkSpace();
	m_normalDebugging = false;m_attachPID = dwPID;
	return true;
}

bool clsDebugger::SuspendDebuggingAll()
{
	for(int i = 0;i < PIDs.size();i++)
		SuspendDebugging(PIDs[i].dwPID);
	return true;
}

bool clsDebugger::SuspendDebugging(DWORD dwPID)
{
	if(CheckProcessState(dwPID))
	{
		if(dbgSettings.dwSuspendType == 0x0)
		{
			HANDLE hProcess = NULL;
			for(int i = 0;i < PIDs.size(); i++)
			{
				if(PIDs[i].bRunning && PIDs[i].dwPID == dwPID)
					hProcess = PIDs[i].hProc;
			}

			if(DebugBreakProcess(hProcess))
			{
				emit OnLog(QString("[!] %1 Debugging suspended!").arg(dwPID, 6, 16, QChar('0')));

				return true;
			}
		}
		else// if(dbgSettings.dwSuspendType == 0x1)
		{
			if(SuspendProcess(dwPID,true))
			{
				emit OnLog(QString("[!] %1 Debugging suspended!").arg(dwPID, 6, 16, QChar('0')));

				return true;
			}
		}
	}
	return false;
}

bool clsDebugger::StopDebuggingAll()
{
	for(int i = 0;i < PIDs.size();i++)
		StopDebugging(PIDs[i].dwPID);
	return SignalDebugEvent();
}

bool clsDebugger::StopDebugging(DWORD dwPID)
{
	HANDLE hProcess = GetCurrentProcessHandle(dwPID);

	if(CheckProcessState(dwPID))
	{
		if(TerminateProcess(hProcess,0))
		{
			return true;
		}
	}
	return false;
}

bool clsDebugger::ResumeDebugging()
{
	bool resumedAnyThread = false;
	for(int i = 0;i < PIDs.size(); i++)
	{
		if(SuspendProcess(PIDs[i].dwPID,false))
			resumedAnyThread = true;
	}

	if(IsBreaking())
		return SignalDebugEvent();

	return resumedAnyThread || GetDebuggingState();
}

bool clsDebugger::GetDebuggingState()
{
	if(m_isDebugging == true)
		return true;
	else
		return false;
}

bool clsDebugger::IsTargetSet()
{
	if(m_targetFile.length() > 0)
		return true;
	return false;
}

bool clsDebugger::StepOver(quint64 dwNewOffset)
{
	if(!IsBreaking())
		return false;

	if(!m_pBreakpointManager->BreakpointAdd(SOFTWARE_BP, NULL, m_currentPID, dwNewOffset, 1, BP_STEPOVER, NULL))
		return false;

	return SignalDebugEvent();
}

bool clsDebugger::StepIn()
{
	if(!IsBreaking())
		return false;

	m_singleStepFlag = true;

#ifdef _AMD64_
	BOOL bIsWOW64 = false;
	if(clsAPIImport::pIsWow64Process)
		clsAPIImport::pIsWow64Process(GetCurrentProcessHandle(),&bIsWOW64);
	
	if(bIsWOW64)
		wowProcessContext.EFlags |= 0x100;
	else
		ProcessContext.EFlags |= 0x100;
#else
	ProcessContext.EFlags |= 0x100;
#endif

	return SignalDebugEvent();
}

void clsDebugger::ClearTarget()
{
	m_targetFile.clear();
}

void clsDebugger::SetTarget(QString sTarget)
{
	m_targetFile = sTarget;
	m_normalDebugging = true;
}

DWORD clsDebugger::GetCurrentPID()
{
	if(s_instance == NULL) return 0;
	if(s_instance->IsDebuggerSuspended())
		return s_instance->m_currentPID;
	else
		return s_instance->GetMainProcessID();
}

DWORD clsDebugger::GetCurrentTID()
{
	if(s_instance == NULL) return 0;
	if(s_instance->IsDebuggerSuspended())
		return s_instance->m_currentTID;
	else
		return s_instance->GetMainThreadID();
}

void clsDebugger::SetCommandLine(QString CommandLine)
{
	m_commandLine = CommandLine;
}

void clsDebugger::ClearCommandLine()
{
	m_commandLine.clear();
}

HANDLE clsDebugger::GetCurrentProcessHandle()
{
	if(IsDebuggerSuspended())
		return m_currentProcess;
	else
		return GetCurrentProcessHandle(-1);
}

HANDLE clsDebugger::GetProcessHandleByPID(DWORD PID)
{
	if(s_instance != NULL)
		return s_instance->GetCurrentProcessHandle(PID);

	return NULL;
}

bool clsDebugger::IsOffsetEIP(quint64 Offset)
{
	if(s_instance == NULL) return false;
#ifdef _AMD64_
	if(s_instance->wowProcessContext.Eip == Offset)
		return true;

	if(s_instance->ProcessContext.Rip == Offset)
		return true;
#else
	if(s_instance->ProcessContext.Eip == Offset)
		return true;
#endif

	return false;
}

QString clsDebugger::GetCMDLine()
{
	return m_commandLine;
}

QString clsDebugger::GetTarget()
{
	return m_targetFile;
}

bool clsDebugger::SetTraceFlagForPID(DWORD dwPID,bool bIsEnabled)
{
	for(int i = 0; i < PIDs.size(); i++)
	{
		if(PIDs[i].dwPID == dwPID)
		{
			PIDs[i].bTraceFlag = bIsEnabled;
			if(bIsEnabled)
			{
				qtDLGTrace::enableStatusBarTimer();
				return StepIn();
			}
			else
			{	
				qtDLGTrace::disableStatusBarTimer();
				return true;
			}
		}
	}
	return false;
}

bool clsDebugger::IsDebuggerSuspended()
{
	return m_debuggerBreak;
}

DWORD clsDebugger::GetMainProcessID()
{
	return m_dbgPI.dwProcessId;
}

DWORD clsDebugger::GetMainThreadID()
{
	return m_dbgPI.dwThreadId;
}
