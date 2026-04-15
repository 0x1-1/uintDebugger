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
#include "clsCrashHandler.h"

#include "dbghelp.h"
#include <strsafe.h>
#include <stdio.h>
#include <time.h>

namespace
{
	LONG g_isHandlingCrash = 0;
	LONG g_symbolsInitialized = 0;

	bool IsIgnoredException(DWORD exceptionCode)
	{
		return exceptionCode == 0x000006ba ||
			   exceptionCode == 0x406d1388 ||
			   exceptionCode == 0xE0000001 ||
			   exceptionCode == 0x000006A6 ||
			   exceptionCode == 0x800706B5 ||
			   exceptionCode == 0x40010006 ||
			   exceptionCode == 0x4001000A;
	}

	const WCHAR* GetExceptionName(DWORD exceptionCode)
	{
		switch(exceptionCode)
		{
		case EXCEPTION_ACCESS_VIOLATION:         return L"Access violation";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return L"Array bounds exceeded";
		case EXCEPTION_BREAKPOINT:               return L"Breakpoint";
		case EXCEPTION_DATATYPE_MISALIGNMENT:    return L"Datatype misalignment";
		case EXCEPTION_FLT_DENORMAL_OPERAND:     return L"Float denormal operand";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return L"Float divide by zero";
		case EXCEPTION_FLT_INEXACT_RESULT:       return L"Float inexact result";
		case EXCEPTION_FLT_INVALID_OPERATION:    return L"Float invalid operation";
		case EXCEPTION_FLT_OVERFLOW:             return L"Float overflow";
		case EXCEPTION_FLT_STACK_CHECK:          return L"Float stack check";
		case EXCEPTION_FLT_UNDERFLOW:            return L"Float underflow";
		case EXCEPTION_ILLEGAL_INSTRUCTION:      return L"Illegal instruction";
		case EXCEPTION_IN_PAGE_ERROR:            return L"In-page error";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:       return L"Integer divide by zero";
		case EXCEPTION_INT_OVERFLOW:             return L"Integer overflow";
		case EXCEPTION_INVALID_DISPOSITION:      return L"Invalid disposition";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return L"Non-continuable exception";
		case EXCEPTION_PRIV_INSTRUCTION:         return L"Privileged instruction";
		case EXCEPTION_SINGLE_STEP:              return L"Single step";
		case EXCEPTION_STACK_OVERFLOW:           return L"Stack overflow";
		case 0xE06D7363:                         return L"Unhandled MSVC C++ exception";
		default:                                 return L"Unknown exception";
		}
	}

	void GetModuleBaseNameFromAddress(PVOID address, WCHAR* moduleName, size_t moduleNameCount)
	{
		moduleName[0] = L'\0';

		HMODULE hModule = NULL;
		if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(address),
			&hModule))
		{
			StringCchCopyW(moduleName, moduleNameCount, L"<unknown>");
			return;
		}

		WCHAR modulePath[MAX_PATH] = {0};
		if(GetModuleFileNameW(hModule, modulePath, ARRAYSIZE(modulePath)) <= 0)
		{
			StringCchCopyW(moduleName, moduleNameCount, L"<unknown>");
			return;
		}

		const WCHAR* fileName = wcsrchr(modulePath, L'\\');
		if(fileName == NULL)
			fileName = modulePath;
		else
			++fileName;

		StringCchCopyW(moduleName, moduleNameCount, fileName);
	}

	bool EnsureSymbolsInitialized()
	{
		if(InterlockedCompareExchange(&g_symbolsInitialized, 1, 0) == 0)
		{
			SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
			if(!SymInitializeW(GetCurrentProcess(), NULL, TRUE))
			{
				InterlockedExchange(&g_symbolsInitialized, -1);
				return false;
			}
		}

		return g_symbolsInitialized == 1;
	}

	void ResolveAddress(PVOID address, WCHAR* resolvedText, size_t resolvedTextCount)
	{
		WCHAR moduleName[MAX_PATH] = {0};
		GetModuleBaseNameFromAddress(address, moduleName, ARRAYSIZE(moduleName));

		if(EnsureSymbolsInitialized())
		{
			BYTE symbolBuffer[sizeof(SYMBOL_INFOW) + (MAX_SYM_NAME * sizeof(WCHAR))] = {0};
			PSYMBOL_INFOW symbol = reinterpret_cast<PSYMBOL_INFOW>(symbolBuffer);
			symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
			symbol->MaxNameLen = MAX_SYM_NAME;

			DWORD64 displacement = 0;
			if(SymFromAddrW(GetCurrentProcess(), reinterpret_cast<DWORD64>(address), &displacement, symbol))
			{
				StringCchPrintfW(resolvedText,
					resolvedTextCount,
					L"%s!%s+0x%I64X",
					moduleName,
					symbol->Name,
					displacement);
				return;
			}
		}

		StringCchPrintfW(resolvedText,
			resolvedTextCount,
			L"%s!0x%p",
			moduleName,
			address);
	}

	void FormatCrashReason(const EXCEPTION_RECORD* exceptionRecord, WCHAR* reasonText, size_t reasonTextCount)
	{
		if(exceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && exceptionRecord->NumberParameters >= 2)
		{
			const WCHAR* accessType = L"access";
			switch(exceptionRecord->ExceptionInformation[0])
			{
			case 0: accessType = L"read"; break;
			case 1: accessType = L"write"; break;
			case 8: accessType = L"execute"; break;
			default: break;
			}

			StringCchPrintfW(reasonText,
				reasonTextCount,
				L"Tried to %s address 0x%p",
				accessType,
				reinterpret_cast<void*>(exceptionRecord->ExceptionInformation[1]));
			return;
		}

		StringCchPrintfW(reasonText,
			reasonTextCount,
			L"%s (0x%08X)",
			GetExceptionName(exceptionRecord->ExceptionCode),
			exceptionRecord->ExceptionCode);
	}

	void GetCrashBasePath(WCHAR* crashBasePath, size_t crashBasePathCount)
	{
		WCHAR exePath[MAX_PATH] = {0};
		GetModuleFileNameW(NULL, exePath, ARRAYSIZE(exePath));

		WCHAR* fileName = wcsrchr(exePath, L'\\');
		if(fileName != NULL)
			*(fileName + 1) = L'\0';

		time_t currentTime = 0;
		tm timeInfo;
		time(&currentTime);
		localtime_s(&timeInfo, &currentTime);

		WCHAR fileBaseName[128] = {0};
		swprintf_s(fileBaseName,
			ARRAYSIZE(fileBaseName),
			L"uintDebugger_%d-%d_%d.%d_crash",
			timeInfo.tm_mday,
			timeInfo.tm_mon + 1,
			timeInfo.tm_hour,
			timeInfo.tm_min);

		StringCchPrintfW(crashBasePath, crashBasePathCount, L"%s%s", exePath, fileBaseName);
	}

	void BuildCrashPath(const WCHAR* extension, WCHAR* outputPath, size_t outputPathCount)
	{
		WCHAR crashBasePath[MAX_PATH] = {0};
		GetCrashBasePath(crashBasePath, ARRAYSIZE(crashBasePath));
		StringCchPrintfW(outputPath, outputPathCount, L"%s.%s", crashBasePath, extension);
	}

	void AppendStackTrace(PCONTEXT contextRecord, WCHAR* reportText, size_t reportTextCount)
	{
		if(contextRecord == NULL)
			return;

		WCHAR resolvedAddress[512] = {0};
		WCHAR stackLine[640] = {0};

#ifdef _AMD64_
		ResolveAddress(reinterpret_cast<PVOID>(contextRecord->Rip), resolvedAddress, ARRAYSIZE(resolvedAddress));
#else
		ResolveAddress(reinterpret_cast<PVOID>(contextRecord->Eip), resolvedAddress, ARRAYSIZE(resolvedAddress));
#endif
		StringCchPrintfW(stackLine, ARRAYSIZE(stackLine), L"  #0 %s\r\n", resolvedAddress);
		StringCchCatW(reportText, reportTextCount, stackLine);

		if(!EnsureSymbolsInitialized())
			return;

		CONTEXT contextCopy = *contextRecord;
		STACKFRAME64 stackFrame = {};
#ifdef _AMD64_
		DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
		stackFrame.AddrPC.Offset = contextCopy.Rip;
		stackFrame.AddrFrame.Offset = contextCopy.Rbp;
		stackFrame.AddrStack.Offset = contextCopy.Rsp;
#else
		DWORD machineType = IMAGE_FILE_MACHINE_I386;
		stackFrame.AddrPC.Offset = contextCopy.Eip;
		stackFrame.AddrFrame.Offset = contextCopy.Ebp;
		stackFrame.AddrStack.Offset = contextCopy.Esp;
#endif
		stackFrame.AddrPC.Mode = AddrModeFlat;
		stackFrame.AddrFrame.Mode = AddrModeFlat;
		stackFrame.AddrStack.Mode = AddrModeFlat;

		DWORD64 lastAddress = stackFrame.AddrPC.Offset;
		for(int frameIndex = 1; frameIndex < 6; ++frameIndex)
		{
			if(!StackWalk64(machineType,
				GetCurrentProcess(),
				GetCurrentThread(),
				&stackFrame,
				&contextCopy,
				NULL,
				SymFunctionTableAccess64,
				SymGetModuleBase64,
				NULL))
			{
				break;
			}

			if(stackFrame.AddrPC.Offset == 0 || stackFrame.AddrPC.Offset == lastAddress)
				break;

			lastAddress = stackFrame.AddrPC.Offset;
			ResolveAddress(reinterpret_cast<PVOID>(stackFrame.AddrPC.Offset), resolvedAddress, ARRAYSIZE(resolvedAddress));
			StringCchPrintfW(stackLine, ARRAYSIZE(stackLine), L"  #%d %s\r\n", frameIndex, resolvedAddress);
			StringCchCatW(reportText, reportTextCount, stackLine);
		}
	}

	void WriteTextReport(const WCHAR* reportPath, const WCHAR* reportText)
	{
		HANDLE reportFile = CreateFileW(reportPath,
			GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if(reportFile == INVALID_HANDLE_VALUE)
			return;

		DWORD written = 0;
		const WORD bom = 0xFEFF;
		WriteFile(reportFile, &bom, sizeof(bom), &written, NULL);
		WriteFile(reportFile, reportText, static_cast<DWORD>(wcslen(reportText) * sizeof(WCHAR)), &written, NULL);
		CloseHandle(reportFile);
	}
}

LONG CALLBACK clsCrashHandler::ErrorReporter(PEXCEPTION_POINTERS pExceptionPtrs)
{
	if(pExceptionPtrs == NULL || pExceptionPtrs->ExceptionRecord == NULL)
		return EXCEPTION_CONTINUE_SEARCH;

	if(IsIgnoredException(pExceptionPtrs->ExceptionRecord->ExceptionCode))
		return EXCEPTION_CONTINUE_EXECUTION;

	if(InterlockedExchange(&g_isHandlingCrash, 1) != 0)
	{
		TerminateProcess(GetCurrentProcess(), 1);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	WCHAR dumpPath[MAX_PATH] = {0};
	WCHAR reportPath[MAX_PATH] = {0};
	WCHAR resolvedAddress[512] = {0};
	WCHAR crashReason[512] = {0};
	WCHAR reportText[8192] = {0};
	WCHAR dumpStatus[256] = {0};

	BuildCrashPath(L"dmp", dumpPath, ARRAYSIZE(dumpPath));
	BuildCrashPath(L"txt", reportPath, ARRAYSIZE(reportPath));

	ResolveAddress(pExceptionPtrs->ExceptionRecord->ExceptionAddress, resolvedAddress, ARRAYSIZE(resolvedAddress));
	FormatCrashReason(pExceptionPtrs->ExceptionRecord, crashReason, ARRAYSIZE(crashReason));

	BOOL dumpWritten = FALSE;
	HANDLE dumpFile = CreateFileW(dumpPath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if(dumpFile != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION dumpInfo = {};
		dumpInfo.ThreadId = GetCurrentThreadId();
		dumpInfo.ExceptionPointers = pExceptionPtrs;
		dumpInfo.ClientPointers = TRUE;

		MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
			MiniDumpWithFullMemoryInfo |
			MiniDumpWithHandleData |
			MiniDumpWithThreadInfo |
			MiniDumpWithUnloadedModules);

		dumpWritten = MiniDumpWriteDump(GetCurrentProcess(),
			GetCurrentProcessId(),
			dumpFile,
			dumpType,
			&dumpInfo,
			NULL,
			NULL);

		CloseHandle(dumpFile);
	}

	StringCchPrintfW(dumpStatus,
		ARRAYSIZE(dumpStatus),
		dumpWritten ? L"Dump: %s\r\nReport: %s" : L"Dump: failed to write\r\nReport: %s",
		dumpWritten ? dumpPath : L"<unavailable>",
		reportPath);

	StringCchPrintfW(reportText,
		ARRAYSIZE(reportText),
		L"uintDebugger hit a fatal exception and must terminate.\r\n\r\n"
		L"Exception: %s (0x%08X)\r\n"
		L"Reason: %s\r\n"
		L"Address: 0x%p\r\n"
		L"Location: %s\r\n"
		L"Thread ID: %lu\r\n"
		L"%s\r\n\r\n"
		L"Stack trace:\r\n",
		GetExceptionName(pExceptionPtrs->ExceptionRecord->ExceptionCode),
		pExceptionPtrs->ExceptionRecord->ExceptionCode,
		crashReason,
		pExceptionPtrs->ExceptionRecord->ExceptionAddress,
		resolvedAddress,
		GetCurrentThreadId(),
		dumpStatus);

	AppendStackTrace(pExceptionPtrs->ContextRecord, reportText, ARRAYSIZE(reportText));
	WriteTextReport(reportPath, reportText);

	MessageBoxW(NULL,
		reportText,
		L"uintDebugger Crash Report",
		MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);

	if(IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;

	TerminateProcess(GetCurrentProcess(), 1);
	return EXCEPTION_EXECUTE_HANDLER;
}
