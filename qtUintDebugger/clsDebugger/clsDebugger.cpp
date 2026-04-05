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
#include "clsAPIImport.h"
#include "clsHelperClass.h"
#include "clsPEManager.h"
#include "clsMemManager.h"

#include "dbghelp.h"

#include <QCoreApplication>
#include <Psapi.h>
#include <TlHelp32.h>

#pragma comment(lib,"psapi.lib")

clsDebugger* clsDebugger::pThis = NULL;

clsDebugger::clsDebugger() :
	m_normalDebugging(true),
	m_isDebugging(false),
	m_stopDebugging(false),
	m_singleStepFlag(false),
	m_debuggerBreak(false),
	m_continueWithException(0),
	m_currentPID(0),
	m_currentTID(0),
	m_currentProcess(nullptr),
	m_attachPID(0),
	m_commandLine("")
{
	setObjectName(QStringLiteral("clsDebugger"));
	pThis = this;

	m_pPEManager			= clsPEManager::GetInstance();
	m_pBreakpointManager	= clsBreakpointManager::GetInstance();

	ZeroMemory(&_si, sizeof(_si));
	_si.cb = sizeof(_si);
	ZeroMemory(&_pi, sizeof(_pi));

	ZeroMemory(&dbgSettings, sizeof(clsDebuggerSettings));

	m_waitForGUI = CreateEvent(NULL,false,false,NULL);
	m_debugEvent = CreateEvent(NULL,false,false,NULL);

	wowProcessContext.ContextFlags	= WOW64_CONTEXT_ALL;
	ProcessContext.ContextFlags		= CONTEXT_ALL;

	//SymSetOptions(SYMOPT_DEBUG);
}

clsDebugger::~clsDebugger()
{
	if(isRunning())
	{
		if(m_isDebugging)
			StopDebuggingAll();

		wait(5000);
	}

	CleanWorkSpace();
	
	CloseHandle(m_waitForGUI);
	CloseHandle(m_debugEvent);
}

void clsDebugger::CleanWorkSpace()
{
	QWriteLocker locker(&m_stateLock);

	for(int i = 0; i < PIDs.size(); ++i)
	{
		SymCleanup(PIDs.at(i).hProc);
		clsMemManager::CFree(PIDs.at(i).sFileName);
	}

	for(int i = 0; i < DLLs.size(); ++i)
	{
		clsMemManager::CFree(DLLs.at(i).sPath);
	}

	m_pBreakpointManager->BreakpointCleanup();

	PIDs.clear();
	DLLs.clear();
	TIDs.clear();

	// Save _pi handles before zeroing so we can compare them against m_dbgPI below.
	const HANDLE piProc   = _pi.hProcess;
	const HANDLE piThread = _pi.hThread;

	if(piProc != NULL && piThread != NULL)
	{
		CloseHandle(piProc);
		CloseHandle(piThread);
		ZeroMemory(&_si, sizeof(_si));
		_si.cb = sizeof(_si);
		ZeroMemory(&_pi, sizeof(_pi));
	}

	// For attach-debugging, _pi is never populated but m_dbgPI holds real handles
	// opened by the debug API. Close them only if not already closed via _pi above.
	if(m_dbgPI.hProcess != NULL && m_dbgPI.hProcess != piProc)
		CloseHandle(m_dbgPI.hProcess);
	if(m_dbgPI.hThread != NULL && m_dbgPI.hThread != piThread)
		CloseHandle(m_dbgPI.hThread);

	ZeroMemory(&m_dbgPI, sizeof(m_dbgPI));
}

void clsDebugger::run()
{
	m_stopDebugging = false;
	if(m_attachPID != 0 && !m_normalDebugging)
	{
		CleanWorkSpace();
		m_isDebugging = true;
		m_singleStepFlag = false;

		AttachedDebugging();
	}	
	else
	{
		if(m_targetFile.length() <= 0 || m_isDebugging)
			return;

		CleanWorkSpace();
		m_isDebugging = true;
		m_singleStepFlag = false;
		
		NormalDebugging();
	}
}

void clsDebugger::AttachedDebugging()
{
	if(CheckProcessState(m_attachPID) && DebugActiveProcess(m_attachPID))
	{
		emit OnLog("[+] Attached to Process");

		DebuggingLoop();
		m_normalDebugging = true;
		return;
	}

	m_isDebugging = false;
	emit OnDebuggerTerminated();
}

void clsDebugger::NormalDebugging()
{
	DWORD dwCreationFlag = 0x2;

	if(dbgSettings.bDebugChilds == true)
		dwCreationFlag = 0x1;

	if(CreateProcess(m_targetFile.toStdWString().c_str(),(LPWSTR)m_commandLine.toStdWString().c_str(),NULL,NULL,false,dwCreationFlag,NULL,NULL,&_si,&_pi))
		DebuggingLoop();
	else
	{
		m_isDebugging = false;
		emit OnDebuggerTerminated();
	}
}

void clsDebugger::DebuggingLoop()
{
	DEBUG_EVENT debug_event = {0};
	EXCEPTION_RECORD *exInfo = &debug_event.u.Exception.ExceptionRecord;
	bool bContinueDebugging = true;
	DWORD dwContinueStatus = DBG_CONTINUE;
	ZeroMemory(&m_dbgPI, sizeof(m_dbgPI));
	QString symbolPath = "";

	if(dbgSettings.bUseMSSymbols)
	{
		symbolPath = QString("srv*%1/symbols*https://msdl.microsoft.com/download/symbols").arg(QCoreApplication::applicationDirPath());
	}

	if(dbgSettings.bKillOnExit)
		DebugSetProcessKillOnExit(true);
	else
		DebugSetProcessKillOnExit(false);

	while(bContinueDebugging && m_isDebugging)
	{ 
		if (!WaitForDebugEvent(&debug_event, INFINITE))
			bContinueDebugging = false;

		if(m_stopDebugging)
		{
			m_stopDebugging = false;
			DebugActiveProcessStop(debug_event.dwProcessId);
			ContinueDebugEvent(debug_event.dwProcessId,debug_event.dwThreadId,DBG_CONTINUE);
			break;
		}

		switch(debug_event.dwDebugEventCode)
		{
		case CREATE_PROCESS_DEBUG_EVENT:
			{
				CloseHandle(debug_event.u.CreateProcessInfo.hFile);

				HANDLE processHandle = debug_event.u.CreateProcessInfo.hProcess;
				
				if(m_dbgPI.hProcess == NULL)
				{
					m_dbgPI.hProcess = processHandle;
					m_dbgPI.hThread = debug_event.u.CreateProcessInfo.hThread;
					m_dbgPI.dwProcessId = debug_event.dwProcessId;
					m_dbgPI.dwThreadId = debug_event.dwThreadId;
				}

				PTCHAR tcDllFilepath = clsHelperClass::GetFileNameFromModuleBase(processHandle, debug_event.u.CreateProcessInfo.lpBaseOfImage);
				QString processPath = QString::fromWCharArray(tcDllFilepath);
				
				PBProcInfo(debug_event.dwProcessId,tcDllFilepath,(quint64)debug_event.u.CreateProcessInfo.lpStartAddress,-1,processHandle, (DWORD64)debug_event.u.CreateProcessInfo.lpBaseOfImage, true);
				PBThreadInfo(debug_event.dwProcessId,clsHelperClass::GetMainThread(debug_event.dwProcessId),(quint64)debug_event.u.CreateProcessInfo.lpStartAddress,false,0,true);

				emit OnNewPID(processPath, debug_event.dwProcessId);
				
				m_pPEManager->OpenFile(processPath, debug_event.dwProcessId, (DWORD64)debug_event.u.CreateProcessInfo.lpBaseOfImage);
								
				PIDStruct *pCurrentPID = GetCurrentPIDDataPointer(debug_event.dwProcessId);
				pCurrentPID->bSymLoad = SymInitialize(processHandle, symbolPath.toStdString().c_str(), false);
				if(pCurrentPID->bSymLoad)
				{
					SymLoadModuleExW(processHandle,NULL,tcDllFilepath,0,(quint64)debug_event.u.CreateProcessInfo.lpBaseOfImage,0,0,0);
				}
				else
				{
					emit OnLog(QString("[!] Could not load symbols for Process(%1)").arg(debug_event.dwProcessId, 6, 16, QChar('0')));
				}

				m_pBreakpointManager->BreakpointInit(debug_event.dwProcessId);
				m_pBreakpointManager->BreakpointAdd(SOFTWARE_BP, NULL, debug_event.dwProcessId, (quint64)debug_event.u.CreateProcessInfo.lpStartAddress, 1, BP_DONOTKEEP, NULL);
				
				if(dbgSettings.bBreakOnTLS)
				{
					QList<quint64> tlsCallback = clsPEManager::getTLSCallbackOffset(processPath,debug_event.dwProcessId);
					if(tlsCallback.length() > 0)
					{
						for(int i = 0; i < tlsCallback.count(); i++)
						{
							m_pBreakpointManager->BreakpointAdd(SOFTWARE_BP, NULL, debug_event.dwProcessId, (quint64)debug_event.u.CreateProcessInfo.lpBaseOfImage + tlsCallback.at(i), 1, BP_KEEP, NULL);
						}
					}						
				}			
				
				if(dbgSettings.bBreakOnNewPID)
					dwContinueStatus = CallBreakDebugger(&debug_event,0);
				
				break;
			}
		case CREATE_THREAD_DEBUG_EVENT:
			{
				PBThreadInfo(debug_event.dwProcessId, debug_event.dwThreadId, (quint64)debug_event.u.CreateThread.lpStartAddress, false, NULL, true);
				m_pBreakpointManager->BreakpointInit(debug_event.dwProcessId, true);

				if(dbgSettings.bBreakOnNewTID)
					dwContinueStatus = CallBreakDebugger(&debug_event,0);

				break;
			}
		case EXIT_THREAD_DEBUG_EVENT:
			{
				PBThreadInfo(debug_event.dwProcessId, debug_event.dwThreadId, NULL, false, debug_event.u.ExitThread.dwExitCode, false);

				if(dbgSettings.bBreakOnExTID)
					dwContinueStatus = CallBreakDebugger(&debug_event,0);
			
				break;
			}
		case EXIT_PROCESS_DEBUG_EVENT:
			{
				PBProcInfo(debug_event.dwProcessId,(PTCHAR)L"",NULL,debug_event.u.ExitProcess.dwExitCode,NULL,NULL, false);
				SymCleanup(GetCurrentProcessHandle(debug_event.dwProcessId));

				emit DeletePEManagerObject("", debug_event.dwProcessId);

				bool bStillOneRunning = false;
				for(int i = 0; i < PIDs.size(); i++)
				{
					if(PIDs[i].bRunning && debug_event.dwProcessId != PIDs[i].dwPID)
					{
						bStillOneRunning = true;
						break;
					}
				}

				if(!bStillOneRunning)
					bContinueDebugging = false;

				if(dbgSettings.bBreakOnExPID)
					dwContinueStatus = CallBreakDebugger(&debug_event,0);
			}
			break;

		case LOAD_DLL_DEBUG_EVENT:
			{
				PIDStruct *pCurrentPID = GetCurrentPIDDataPointer(debug_event.dwProcessId);
				PTCHAR sDLLFileName = clsHelperClass::GetFileNameFromModuleBase(pCurrentPID->hProc, debug_event.u.LoadDll.lpBaseOfDll); 
				PBDLLInfo(sDLLFileName,debug_event.dwProcessId,(quint64)debug_event.u.LoadDll.lpBaseOfDll,true);

				if(pCurrentPID->bSymLoad && dbgSettings.bAutoLoadSymbols)
				{
					SymLoadModuleExW(pCurrentPID->hProc,NULL,sDLLFileName,0,(quint64)debug_event.u.LoadDll.lpBaseOfDll,0,0,0);
				}
				else if(pCurrentPID->bSymLoad && !dbgSettings.bAutoLoadSymbols)
				{
					// Auto-load symbols is disabled by user — not an error.
				}
				else
				{
					emit OnLog(QString("[!] Could not load symbols for DLL: ").append(QString::fromWCharArray(sDLLFileName)));
				}

				if(dbgSettings.bBreakOnNewDLL)
					dwContinueStatus = CallBreakDebugger(&debug_event,0);

				CloseHandle(debug_event.u.LoadDll.hFile);
			}
			break;

		case UNLOAD_DLL_DEBUG_EVENT:
			{
				DLLStruct *pCurrent = NULL;
				size_t countDLL = DLLs.size();

				for(int i = 0; i < countDLL; i++)
				{
					pCurrent = &DLLs[i];

					if(pCurrent->dwBaseAdr == (quint64)debug_event.u.UnloadDll.lpBaseOfDll && pCurrent->dwPID == debug_event.dwProcessId)
					{
						PBDLLInfo(NULL, NULL, NULL, false, pCurrent);
						SymUnloadModule64(GetCurrentProcessHandle(debug_event.dwProcessId), pCurrent->dwBaseAdr);
						break;
					}
				}

				if(dbgSettings.bBreakOnExDLL)
					dwContinueStatus = CallBreakDebugger(&debug_event,0);

				break;
			}
		case OUTPUT_DEBUG_STRING_EVENT:
			{
				const WORD msgLen = debug_event.u.DebugString.nDebugStringLength;
				if(msgLen == 0) break;

				PTCHAR wMsg = (PTCHAR)clsMemManager::CAlloc(msgLen * sizeof(TCHAR));
				if(wMsg == NULL) break;

				HANDLE hProcess = GetCurrentProcessHandle(debug_event.dwProcessId);

				if(debug_event.u.DebugString.fUnicode)
				{
					// nDebugStringLength is in characters for Unicode; TCHAR is WCHAR on this build.
					ReadProcessMemory(hProcess,debug_event.u.DebugString.lpDebugStringData,wMsg,msgLen,NULL);
				}
				else
				{
					size_t countConverted = 0;
					PCHAR Msg = (PCHAR)clsMemManager::CAlloc(msgLen * sizeof(CHAR));
					if(Msg != NULL)
					{
						ReadProcessMemory(hProcess,debug_event.u.DebugString.lpDebugStringData,Msg,msgLen,NULL);
						mbstowcs_s(&countConverted,wMsg,msgLen,Msg,msgLen);
						clsMemManager::CFree(Msg);
					}
				}

				PBDbgString(wMsg,debug_event.dwProcessId);

				break;
			}
		case EXCEPTION_DEBUG_EVENT:
			{
				bool	bIsEP			= false,
						bIsBP			= false,
						bIsKernelBP		= false;
				PIDStruct *pCurrentPID	= GetCurrentPIDDataPointer(debug_event.dwProcessId);

				switch (exInfo->ExceptionCode)
				{
				case STATUS_WX86_BREAKPOINT: // Breakpoint in x86 process running under WOW64
					if(pCurrentPID->bKernelBP && !pCurrentPID->bWOW64KernelBP)
					{
						emit OnLog(QString("[!] WOW64 Kernel EP - PID %1 - %2")
							.arg(debug_event.dwProcessId, 6, 16, QChar('0'))
#ifdef _AMD64_
							.arg((DWORD64)exInfo->ExceptionAddress, 16, 16, QChar('0')));
#else
							.arg((DWORD)exInfo->ExceptionAddress, 8, 16, QChar('0')));
#endif
							
						if(dbgSettings.bBreakOnSystemEP)
						{
							dwContinueStatus = CallBreakDebugger(&debug_event,0);
						}
						else
						{
							dwContinueStatus = CallBreakDebugger(&debug_event,3);
						}

						pCurrentPID->bWOW64KernelBP = true;
						bIsKernelBP = true;
						[[fallthrough]]; // WOW64 BP also needs the standard BP handling below
					}
				case EXCEPTION_BREAKPOINT:
					{
						bool bStepOver = false;

						if(!pCurrentPID->bKernelBP)
						{
							emit OnLog(QString("[!] Kernel EP - PID %1 - %2")
								.arg(debug_event.dwProcessId, 6, 16, QChar('0'))
#ifdef _AMD64_
								.arg((DWORD64)exInfo->ExceptionAddress, 16, 16, QChar('0')));
#else
								.arg((DWORD)exInfo->ExceptionAddress, 8, 16, QChar('0')));
#endif
								
							if(dbgSettings.bBreakOnSystemEP)
								dwContinueStatus = CallBreakDebugger(&debug_event,0);
							else
								dwContinueStatus = CallBreakDebugger(&debug_event,3);

							pCurrentPID->bKernelBP = true;
							bIsKernelBP = true;
						}
						else if(pCurrentPID->bKernelBP)
						{
							if((quint64)exInfo->ExceptionAddress == pCurrentPID->dwEP)
							{
								bIsEP = true;
								
								QString tempFilePath = QString::fromWCharArray(pCurrentPID->sFileName);

								m_pPEManager->CloseFile(tempFilePath, pCurrentPID->dwPID);
								m_pPEManager->OpenFile(tempFilePath, pCurrentPID->dwPID, pCurrentPID->imageBase);

								m_pBreakpointManager->BreakpointUpdateOffsets(pCurrentPID->hProc,pCurrentPID->dwPID);
								emit UpdateOffsetsPatches(pCurrentPID->hProc,pCurrentPID->dwPID);

								m_pBreakpointManager->BreakpointInit(debug_event.dwProcessId);
							}

							bIsBP = CheckIfExceptionIsBP(pCurrentPID, (quint64)exInfo->ExceptionAddress, EXCEPTION_BREAKPOINT, true);
							if(!bIsBP)
							{
								break;
							}

							bool bpNeedsReplace = false;
							BPStruct *pCurrentBP;

							if(m_pBreakpointManager->BreakpointFind((DWORD64)exInfo->ExceptionAddress, SOFTWARE_BP, debug_event.dwProcessId, true, &pCurrentBP))
							{
								if(!WriteProcessMemory(pCurrentPID->hProc, (LPVOID)pCurrentBP->dwOffset, (LPVOID)pCurrentBP->bOrgByte, pCurrentBP->dwSize,NULL))
								{
									dwContinueStatus = CallBreakDebugger(&debug_event,0);
									break;
								}
								FlushInstructionCache(pCurrentPID->hProc, (LPVOID)pCurrentBP->dwOffset, pCurrentBP->dwSize);

								switch(pCurrentBP->dwHandle)
								{
								case BP_KEEP: // normal breakpoint
									pCurrentBP->bRestoreBP = true;
									bpNeedsReplace = true;

									break;
								case BP_DONOTKEEP:
								case BP_STEPOVER: // StepOver BP
									if(!bIsEP)
										bStepOver = true;

									m_pBreakpointManager->BreakpointRemove(pCurrentBP->dwOffset,SOFTWARE_BP);
									break;
								case BP_TRACETO: // Trace End BP
									m_singleStepFlag = false;

									m_pBreakpointManager->BreakpointRemove(pCurrentBP->dwOffset,SOFTWARE_BP);
									break;

								default:
									pCurrentBP->bRestoreBP = true;
									break;
								}
							}
								
							if(bpNeedsReplace || bStepOver)
							{
								SetThreadContextHelper(true, true, debug_event.dwThreadId, pCurrentPID);
								pCurrentPID->bTrapFlag = true;
								pCurrentPID->dwBPRestoreFlag = RESTORE_BP_SOFTWARE;
							}
							else
							{
								SetThreadContextHelper(true, false, debug_event.dwThreadId, pCurrentPID);
							}

							if(bIsEP && !dbgSettings.bBreakOnModuleEP)
							{
								dwContinueStatus = CallBreakDebugger(&debug_event,2);
							}
							else
							{
								if(!bStepOver)
								{
									if(bIsEP)
									{
										emit OnLog(QString("[!] Break on - Entrypoint - PID %1 - %2")
											.arg(debug_event.dwProcessId, 6, 16, QChar('0'))
#ifdef _AMD64_
											.arg((DWORD64)exInfo->ExceptionAddress, 16, 16, QChar('0')));
#else
											.arg((DWORD)exInfo->ExceptionAddress, 8, 16, QChar('0')));
#endif
									}
									else
									{
										emit OnLog(QString("[!] Break on - Software BP - PID %1 - %2")
											.arg(debug_event.dwProcessId, 6, 16, QChar('0'))
#ifdef _AMD64_
											.arg((DWORD64)exInfo->ExceptionAddress, 16, 16, QChar('0')));
#else
											.arg((DWORD)exInfo->ExceptionAddress, 8, 16, QChar('0')));
#endif
									}
								}

								dwContinueStatus = CallBreakDebugger(&debug_event,0);
							}
						}
						break;
					}
				case STATUS_PRIVILEGED_INSTRUCTION: // software bp - hlt
				case STATUS_ILLEGAL_INSTRUCTION: // software bp - ud2
					{
						bIsBP = CheckIfExceptionIsBP(pCurrentPID, (quint64)exInfo->ExceptionAddress, EXCEPTION_BREAKPOINT, true);
						if(!bIsBP)
						{
							break;
						}

						BPStruct *pCurrentBP;
						if(m_pBreakpointManager->BreakpointFind((DWORD64)exInfo->ExceptionAddress, SOFTWARE_BP, debug_event.dwProcessId, true, &pCurrentBP))
						{
							if(!WriteProcessMemory(pCurrentPID->hProc, (LPVOID)pCurrentBP->dwOffset, (LPVOID)pCurrentBP->bOrgByte, pCurrentBP->dwSize,NULL))
							{
								dwContinueStatus = CallBreakDebugger(&debug_event,0);
								break;
							}
							FlushInstructionCache(pCurrentPID->hProc, (LPVOID)pCurrentBP->dwOffset, pCurrentBP->dwSize);

							pCurrentBP->bRestoreBP = true;
						}
								
						SetThreadContextHelper(false, true, debug_event.dwThreadId, pCurrentPID);
						pCurrentPID->bTrapFlag = true;
						pCurrentPID->dwBPRestoreFlag = RESTORE_BP_SOFTWARE;
								
						emit OnLog(QString("[!] Break on - Software BP - PID %1 - %2")
								.arg(debug_event.dwProcessId, 6, 16, QChar('0'))
#ifdef _AMD64_
								.arg((DWORD64)exInfo->ExceptionAddress, 16, 16, QChar('0')));
#else
								.arg((DWORD)exInfo->ExceptionAddress, 8, 16, QChar('0')));
#endif
					
						dwContinueStatus = CallBreakDebugger(&debug_event,0);

						break;
					}
				case STATUS_WX86_SINGLE_STEP: // Single step in x86 process running under WOW64
				case EXCEPTION_SINGLE_STEP:
					{
						if(pCurrentPID->bTraceFlag && m_singleStepFlag)
						{
							bIsBP = true;
							SetThreadContextHelper(false, true, debug_event.dwThreadId, pCurrentPID);
							qtDLGTrace::addTraceData((quint64)exInfo->ExceptionAddress, debug_event.dwProcessId, debug_event.dwThreadId);
							break;
						}
						else if(m_singleStepFlag)
						{
							m_singleStepFlag = false;
							bIsBP = true;

							dwContinueStatus = CallBreakDebugger(&debug_event,0);
							break;
						}

						bIsBP = CheckIfExceptionIsBP(pCurrentPID, (quint64)exInfo->ExceptionAddress, EXCEPTION_SINGLE_STEP, true, false);
						if(!bIsBP)
						{
							break;
						}

						if(pCurrentPID->dwBPRestoreFlag == RESTORE_BP_SOFTWARE) // Restore SoftwareBP
						{
							BPStruct *pCurrentBP = NULL;
							int countSoftwareBP = m_pBreakpointManager->SoftwareBPs.size();

							for(int i = 0; i < countSoftwareBP; i++)
							{
								pCurrentBP = &m_pBreakpointManager->SoftwareBPs[i];

								if(pCurrentBP->bRestoreBP &&
									pCurrentBP->dwHandle == BP_KEEP &&
									(pCurrentBP->dwPID == debug_event.dwProcessId || pCurrentBP->dwPID == -1))
								{
									clsBreakpointSoftware::wSoftwareBP(pCurrentBP->dwPID, pCurrentBP->dwOffset, pCurrentBP->dwSize, &pCurrentBP->bOrgByte, pCurrentBP->dwDataType);
									pCurrentBP->bRestoreBP = false;

									break;
								}									
							}

							pCurrentPID->bTrapFlag = false;
							pCurrentPID->dwBPRestoreFlag = RESTORE_NON;
						}
						else if(pCurrentPID->dwBPRestoreFlag == RESTORE_BP_MEMORY) // Restore MemBP
						{
							BPStruct *pCurrentBP = NULL;
							int countMemoryBP = m_pBreakpointManager->MemoryBPs.size();

							for(int i = 0; i < countMemoryBP; i++)
							{
								pCurrentBP = &m_pBreakpointManager->MemoryBPs[i];

								if(pCurrentBP->bRestoreBP &&
									pCurrentBP->dwHandle == BP_KEEP &&
									(pCurrentBP->dwPID == debug_event.dwProcessId || pCurrentBP->dwPID == -1))
								{
									clsBreakpointMemory::wMemoryBP(pCurrentBP->dwPID, pCurrentBP->dwOffset, pCurrentBP->dwSize, pCurrentBP->dwTypeFlag, &pCurrentBP->dwOldProtection);
									pCurrentBP->bRestoreBP = false;

									break;
								}									
							}

							pCurrentPID->bTrapFlag = false;
							pCurrentPID->dwBPRestoreFlag = RESTORE_NON;
						}
						else if(pCurrentPID->dwBPRestoreFlag == RESTORE_BP_HARDWARE) // Restore HwBp
						{
							BPStruct *pCurrentBP = NULL;
							int countHardwareBP = m_pBreakpointManager->HardwareBPs.size();

							for(int i = 0; i < countHardwareBP; i++)
							{
								pCurrentBP = &m_pBreakpointManager->HardwareBPs[i];

								if(pCurrentBP->bRestoreBP &&
									pCurrentBP->dwHandle == BP_KEEP &&
									(pCurrentBP->dwPID == debug_event.dwProcessId || pCurrentBP->dwPID == -1))
								{
									clsBreakpointHardware::wHardwareBP(debug_event.dwProcessId, pCurrentBP->dwOffset, pCurrentBP->dwSize, pCurrentBP->dwSlot, pCurrentBP->dwTypeFlag);
									pCurrentBP->bRestoreBP = false;

									break;
								}
							}

							pCurrentPID->bTrapFlag = false;
							pCurrentPID->dwBPRestoreFlag = RESTORE_NON;
						}
						else if(pCurrentPID->dwBPRestoreFlag == RESTORE_NON) // First time hit HwBP
						{
							BPStruct *pCurrentBP;
							if(m_pBreakpointManager->BreakpointFind((DWORD64)exInfo->ExceptionAddress, HARDWARE_BP, debug_event.dwProcessId, true, &pCurrentBP))
							{
								clsBreakpointHardware::dHardwareBP(debug_event.dwProcessId, pCurrentBP->dwOffset, pCurrentBP->dwSlot);

								emit OnLog(QString("[!] Break on - Hardware BP - PID %1 - %2")
									.arg(debug_event.dwProcessId, 6, 16, QChar('0'))
#ifdef _AMD64_
									.arg((DWORD64)pCurrentBP->dwOffset, 16, 16, QChar('0')));
#else
									.arg((DWORD)pCurrentBP->dwOffset, 8, 16, QChar('0')));
#endif

								pCurrentBP->bRestoreBP = true;
								pCurrentPID->dwBPRestoreFlag = RESTORE_BP_HARDWARE;
								pCurrentPID->bTrapFlag = true;
									
								dwContinueStatus = CallBreakDebugger(&debug_event,0);

								SetThreadContextHelper(false, true, debug_event.dwThreadId, pCurrentPID);
							}
						}
						else
						{
							bIsBP = false;
						}

						break;
					}
				case EXCEPTION_ACCESS_VIOLATION:
				case EXCEPTION_GUARD_PAGE:
					{
						bIsBP = CheckIfExceptionIsBP(pCurrentPID, (quint64)exInfo->ExceptionAddress, exInfo->ExceptionCode, true);
						if(!bIsBP)
						{
							break;
						}

						SetThreadContextHelper(false, true, debug_event.dwThreadId, pCurrentPID);
						pCurrentPID->dwBPRestoreFlag = RESTORE_BP_MEMORY;
						pCurrentPID->bTrapFlag = true;

						BPStruct *pCurrentBP;
						if(m_pBreakpointManager->BreakpointFind((DWORD64)exInfo->ExceptionAddress, MEMORY_BP, debug_event.dwProcessId, true, &pCurrentBP))
						{
							pCurrentBP->bRestoreBP = true;

							emit OnLog(QString("[!] Break on - Memory BP - PID %1 - %2")
								.arg(debug_event.dwProcessId, 6, 16, QChar('0'))
#ifdef _AMD64_
								.arg((DWORD64)pCurrentBP->dwOffset, 16, 16, QChar('0')));
#else
								.arg((DWORD)pCurrentBP->dwOffset, 8, 16, QChar('0')));
#endif

							if(exInfo->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
							{
								DWORD currentProtection = NULL;
								VirtualProtectEx(pCurrentPID->hProc, (LPVOID)exInfo->ExceptionAddress, pCurrentBP->dwSize, pCurrentBP->dwOldProtection, &currentProtection);
							}

							dwContinueStatus = CallBreakDebugger(&debug_event,0);
							break;							
						}
						else
						{ // PAGE_GUARD on a page where we placed an BP
							MEMORY_BASIC_INFORMATION mbi;

							quint64 pageBase = NULL,
									pageSize = NULL;

							HANDLE processHandle = pCurrentPID->hProc;

							if(VirtualQueryEx(processHandle,(LPVOID)exInfo->ExceptionAddress,&mbi,sizeof(mbi)))
							{
								pageSize = mbi.RegionSize; 
								pageBase = (DWORD64)mbi.BaseAddress;
							}

							for(int i = 0;i < m_pBreakpointManager->MemoryBPs.size(); i++)
							{
								if(m_pBreakpointManager->MemoryBPs[i].dwOffset <= (pageBase + pageSize) && m_pBreakpointManager->MemoryBPs[i].dwOffset >= pageBase)
								{
									m_pBreakpointManager->MemoryBPs[i].bRestoreBP = true;

									if(exInfo->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
									{
										DWORD currentProtection = NULL;
										VirtualProtectEx(pCurrentPID->hProc, (LPVOID)exInfo->ExceptionAddress, m_pBreakpointManager->MemoryBPs[i].dwSize, m_pBreakpointManager->MemoryBPs[i].dwOldProtection, &currentProtection);
									}

									break;
								}
							}
						}
						break;
					}
				}

				if(!bIsEP && !bIsKernelBP && !bIsBP)
				{
					bool bExceptionHandler = false;

					PBExceptionInfo((quint64)exInfo->ExceptionAddress, exInfo->ExceptionCode, debug_event.dwProcessId, debug_event.dwThreadId);

					for (int i = 0; i < ExceptionHandler.size(); i++)
					{
						if(exInfo->ExceptionCode == ExceptionHandler[i].dwExceptionType)
						{
							bExceptionHandler = true;

							if(ExceptionHandler[i].dwHandler != NULL)
							{
								CustomHandler pCustomHandler = (CustomHandler)ExceptionHandler[i].dwHandler;
								dwContinueStatus = pCustomHandler(&debug_event);

								if(ExceptionHandler[i].dwAction == 0)
									dwContinueStatus = CallBreakDebugger(&debug_event, 0);
							}
							else
								dwContinueStatus = CallBreakDebugger(&debug_event,ExceptionHandler[i].dwAction);

							break; // remove this to allow multiple exception handlers for the same event
						}
					}

					if(!bExceptionHandler)
					{
						if(dbgSettings.dwDefaultExceptionMode == 1)
						{
							dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
						}
						else if(dbgSettings.bUseExceptionAssist)
						{
							m_continueWithException = 0;
							emit AskForException((DWORD)exInfo->ExceptionCode);

							// 30-second timeout prevents permanent deadlock if the
							// exception assistant dialog is closed without responding.
							if(WaitForSingleObject(m_waitForGUI, 30000) == WAIT_TIMEOUT)
								m_continueWithException = 0; // default: pass exception to app

							if(m_continueWithException >= 10)
							{
								m_continueWithException -= 10;
								CustomExceptionAdd((DWORD)exInfo->ExceptionCode, m_continueWithException, NULL);
							}

							dwContinueStatus = CallBreakDebugger(&debug_event,m_continueWithException);
						}
						else
						{
							dwContinueStatus = CallBreakDebugger(&debug_event,0);
						}
					}
				}
			}			
			break;
		}

		ContinueDebugEvent(debug_event.dwProcessId,debug_event.dwThreadId,dwContinueStatus);
		dwContinueStatus = DBG_CONTINUE;
	}

	m_isDebugging = false;

	emit OnLog("[-] Debugging finished!");

	CleanWorkSpace();

	emit OnDebuggerTerminated();
}

DWORD clsDebugger::CallBreakDebugger(DEBUG_EVENT *debug_event,DWORD dwHandle)
{
	switch(dwHandle)
	{
	case 0:
		{
			HANDLE hThread = OpenThread(THREAD_GETSET_CONTEXT,false,debug_event->dwThreadId);
			m_currentPID = debug_event->dwProcessId;
			m_currentTID = debug_event->dwThreadId;
			m_currentProcess = GetCurrentProcessHandle(debug_event->dwProcessId);
			m_debuggerBreak = true;

#ifdef _AMD64_
			BOOL bIsWOW64 = false;

			if(clsAPIImport::pIsWow64Process)
				clsAPIImport::pIsWow64Process(m_currentProcess,&bIsWOW64);
			if(bIsWOW64)
			{
				clsAPIImport::pWow64GetThreadContext(hThread,&wowProcessContext);

				emit OnDebuggerBreak();
				WaitForSingleObject(m_debugEvent,INFINITE);

				clsAPIImport::pWow64SetThreadContext(hThread,&wowProcessContext);
			}
			else
			{
				GetThreadContext(hThread,&ProcessContext);

				emit OnDebuggerBreak();
				WaitForSingleObject(m_debugEvent,INFINITE);

				SetThreadContext(hThread,&ProcessContext);
			}

#else
			GetThreadContext(hThread,&ProcessContext);

			emit OnDebuggerBreak();
			WaitForSingleObject(m_debugEvent,INFINITE);

			SetThreadContext(hThread,&ProcessContext);
#endif
			m_currentPID    = 0;
			m_currentTID    = 0;
			m_currentProcess = nullptr;
			m_debuggerBreak  = false;

			CloseHandle(hThread);
			return DBG_EXCEPTION_HANDLED;
		}
	case 1:
		return DBG_EXCEPTION_NOT_HANDLED;
	case 2:
		return DBG_CONTINUE;
	case 3:
		return DBG_EXCEPTION_HANDLED;
	default:
		return DBG_EXCEPTION_NOT_HANDLED;
	}
}

bool clsDebugger::CheckProcessState(DWORD dwPID)
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 procEntry32;

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);

	if(hProcessSnap == INVALID_HANDLE_VALUE)
		return false;
	procEntry32.dwSize = sizeof(PROCESSENTRY32);

	if(!Process32First(hProcessSnap,&procEntry32))
	{
		CloseHandle(hProcessSnap);
		return false;
	}

	do{
		if(procEntry32.th32ProcessID == dwPID)
		{
			CloseHandle(hProcessSnap);
			return true;
		}
	}while(Process32Next(hProcessSnap,&procEntry32));

	CloseHandle(hProcessSnap);
	return false;
}

bool clsDebugger::CheckIfExceptionIsBP(PIDStruct *pCurrentPID, quint64 dwExceptionOffset,quint64 dwExceptionType, bool bClearTrapFlag, bool isExceptionRelevant)
{
	if(pCurrentPID->bTrapFlag)
	{
		if(bClearTrapFlag)
			pCurrentPID->bTrapFlag = false;
		return true;
	}
	else if((dwExceptionType == EXCEPTION_SINGLE_STEP || dwExceptionType == STATUS_WX86_SINGLE_STEP) && m_singleStepFlag)
	{
		return isExceptionRelevant;
	}
	else if(dwExceptionType == EXCEPTION_BREAKPOINT || dwExceptionType == STATUS_WX86_BREAKPOINT)
	{
		for(int i = 0;i < m_pBreakpointManager->SoftwareBPs.size();i++)
			if(dwExceptionOffset == m_pBreakpointManager->SoftwareBPs[i].dwOffset && (m_pBreakpointManager->SoftwareBPs[i].dwPID == pCurrentPID->dwPID || m_pBreakpointManager->SoftwareBPs[i].dwPID == -1))
				return true;		
	}
	else if(dwExceptionType == EXCEPTION_GUARD_PAGE || dwExceptionType == EXCEPTION_ACCESS_VIOLATION)
	{
		DWORD64 pageBase = NULL,
				pageSize = NULL;

		MEMORY_BASIC_INFORMATION mbi;

		if(VirtualQueryEx(pCurrentPID->hProc,(LPVOID)dwExceptionOffset,&mbi,sizeof(mbi)))
		{
			pageSize = mbi.RegionSize; 
			pageBase = (DWORD64)mbi.BaseAddress;
		}

		for(int i = 0;i < m_pBreakpointManager->MemoryBPs.size(); i++)
		{
			if(m_pBreakpointManager->MemoryBPs[i].dwOffset <= (pageBase + pageSize) && m_pBreakpointManager->MemoryBPs[i].dwOffset >= pageBase)
				return true;
		}
	}
	else if(dwExceptionType == STATUS_WX86_SINGLE_STEP || dwExceptionType == EXCEPTION_SINGLE_STEP)
	{
		for(int i = 0;i < m_pBreakpointManager->HardwareBPs.size();i++)
			if(dwExceptionOffset == m_pBreakpointManager->HardwareBPs[i].dwOffset && (m_pBreakpointManager->HardwareBPs[i].dwPID == pCurrentPID->dwPID || m_pBreakpointManager->HardwareBPs[i].dwPID == -1))
				return true;
	}

	return false;
}

bool clsDebugger::SuspendProcess(DWORD dwPID,bool bSuspend)
{
	HANDLE hProcessSnap;
	THREADENTRY32 threadEntry32;
	threadEntry32.dwSize = sizeof(THREADENTRY32);

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,dwPID);

	if(hProcessSnap == INVALID_HANDLE_VALUE)
		return false;

	if(!Thread32First(hProcessSnap,&threadEntry32))
	{
		CloseHandle(hProcessSnap);
		return false;
	}

	do{
		HANDLE hThread = INVALID_HANDLE_VALUE;

		if(dwPID == threadEntry32.th32OwnerProcessID)
			hThread = OpenThread(THREAD_SUSPEND_RESUME ,false,threadEntry32.th32ThreadID);

		if(hThread != INVALID_HANDLE_VALUE)
		{
			if(bSuspend)
				SuspendThread(hThread);
			else
				ResumeThread(hThread);

			CloseHandle(hThread);
		}
	}while(Thread32Next(hProcessSnap,&threadEntry32));

	CloseHandle(hProcessSnap);
	return true;
}

void clsDebugger::CustomExceptionAdd(DWORD dwExceptionType,DWORD dwAction,quint64 dwHandler)
{
	customException custEx;
	custEx.dwAction = dwAction;
	custEx.dwExceptionType = dwExceptionType;
	custEx.dwHandler = dwHandler;
	QWriteLocker locker(&m_stateLock);
	ExceptionHandler.append(custEx);
}

void clsDebugger::CustomExceptionRemove(DWORD dwExceptionType)
{
	QWriteLocker locker(&m_stateLock);
	for (QVector<customException>::iterator it = ExceptionHandler.begin(); it != ExceptionHandler.end(); )
	{
		if(it->dwExceptionType == dwExceptionType)
			it = ExceptionHandler.erase(it);
		else
			++it;
	}
}

void clsDebugger::CustomExceptionRemoveAll()
{
	QWriteLocker locker(&m_stateLock);
	ExceptionHandler.clear();
}

bool clsDebugger::SetThreadContextHelper(bool bDecIP, bool bSetTrapFlag, DWORD dwThreadID, PIDStruct *pCurrentPID)
{
	HANDLE hThread = OpenThread(THREAD_GETSET_CONTEXT, false, dwThreadID);
	if(hThread == INVALID_HANDLE_VALUE) 
		return false;

#ifdef _AMD64_
	BOOL bIsWOW64 = false;

	if(clsAPIImport::pIsWow64Process)
		clsAPIImport::pIsWow64Process(pCurrentPID->hProc, &bIsWOW64);

	if(bIsWOW64)
	{
		WOW64_CONTEXT wowcTT;
		wowcTT.ContextFlags = WOW64_CONTEXT_ALL;
		clsAPIImport::pWow64GetThreadContext(hThread, &wowcTT);

		if(bDecIP)
			wowcTT.Eip--;

		if(bSetTrapFlag)
			wowcTT.EFlags |= 0x100;
		else
			wowcTT.EFlags &= ~0x100;

		clsAPIImport::pWow64SetThreadContext(hThread, &wowcTT);
	}
	else
	{
		CONTEXT cTT;
		cTT.ContextFlags = CONTEXT_ALL;
		GetThreadContext(hThread, &cTT);

		if(bDecIP)
			cTT.Rip--;

		if(bSetTrapFlag)
			cTT.EFlags |= 0x100;
		else
			cTT.EFlags &= ~0x100;

		SetThreadContext(hThread, &cTT);
	}

#else
	CONTEXT cTT;
	cTT.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hThread, &cTT);

	if(bDecIP)
		cTT.Eip--;

	if(bSetTrapFlag)
		cTT.EFlags |= 0x100;
	else
		cTT.EFlags &= ~0x100;

	SetThreadContext(hThread, &cTT);
#endif

	CloseHandle(hThread);
	return true;
}

HANDLE clsDebugger::GetCurrentProcessHandle(DWORD dwPID)
{
	for(int i = 0; i < PIDs.size(); i++)
	{
		if(PIDs[i].dwPID == dwPID)
			return PIDs[i].hProc;
	}
	return m_dbgPI.hProcess;
}

void clsDebugger::HandleForException(int handleException)
{
	m_continueWithException = handleException;
	SetEvent(m_waitForGUI);
}

void clsDebugger::SetNewThreadContext(bool isWow64, CONTEXT newProcessContext, WOW64_CONTEXT newWowProcessContext)
{
	if(isWow64)
		pThis->wowProcessContext = newWowProcessContext;
	else
		pThis->ProcessContext = newProcessContext;
}

PIDStruct* clsDebugger::GetCurrentPIDDataPointer(DWORD processID)
{
	PIDStruct *pCurrentPID = NULL;
	int countPID = PIDs.size();

	for(int i = 0;i < countPID; i++)
	{
		pCurrentPID = &PIDs[i];

		if(pCurrentPID->dwPID == processID)
		{
			return pCurrentPID;
		}
	}

	return pCurrentPID; // should never happen
}
