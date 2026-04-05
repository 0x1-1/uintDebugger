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
#ifndef CLSBREAKPOINTMANAGER_H
#define CLSBREAKPOINTMANAGER_H

#include "clsBreakpointHardware.h"
#include "clsBreakpointMemory.h"
#include "clsBreakpointSoftware.h"

#include <Windows.h>
#include <QObject>
#include <QList>
#include <QReadWriteLock>
	 
struct BPStruct
{
	DWORD dwSize;
	DWORD dwSlot;
	DWORD dwTypeFlag;	/* see BP_BREAKON		*/
	DWORD dwHandle;		/* see BREAKPOINT_TYPE	*/
	DWORD dwOldProtection;
	DWORD dwDataType;
	DWORD dwHitCount;   /* runtime: how many times this BP has fired (not persisted) */
	DWORD dwHitTarget;  /* break only on this hit number; 0 = break every time        */
	DWORD dwTID;        /* break only on this thread ID; 0 = any thread               */
	quint64 dwOffset;
	quint64 dwBaseOffset;
	quint64 dwOldOffset;
	int dwPID;
	PBYTE bOrgByte;
	bool bRestoreBP;
	PTCHAR moduleName;
	PTCHAR comment;     /* optional user label; heap-allocated like moduleName, NULL = none */
};

class clsBreakpointManager : public QObject
{
	Q_OBJECT

public:
	QList<BPStruct> SoftwareBPs;
	QList<BPStruct> MemoryBPs;
	QList<BPStruct> HardwareBPs;

	// Lock protecting SoftwareBPs, MemoryBPs, HardwareBPs.
	// Public so the debug loop can take a read lock while iterating them.
	// Take write lock for structural mutations (append/remove);
	// read lock for iteration without structural change.
	QReadWriteLock m_bpLock;

	clsBreakpointManager();
	~clsBreakpointManager();

	bool BreakpointRemove(DWORD64 breakpointOffset, DWORD breakpointType);
	bool BreakpointClear();
	bool BreakpointAdd(DWORD breakpointType, DWORD typeFlag, DWORD processID, DWORD64 breakpointOffset, int breakpointSize, DWORD breakpointHandleType, DWORD breakpointDataType, DWORD hitTarget = 0, DWORD tid = 0);
	bool BreakpointInit(DWORD processID, bool isThread = false);
	bool BreakpointFind(DWORD64 breakpointOffset, int breakpointType, DWORD processID, bool takeAll, BPStruct** pBreakpointSearched);

	void BreakpointCleanup();
	void BreakpointUpdateOffsets(HANDLE processHandle, DWORD processID);

	bool SetBPComment(DWORD64 offset, DWORD bpType, const QString &comment);

	static bool IsOffsetAnBP(quint64 Offset);
	static bool BreakpointInsert(DWORD breakpointType, DWORD typeFlag, DWORD processID, DWORD64 breakpointOffset, int breakpointSize, DWORD breakpointHandleType, DWORD breakpointDataType = NULL, DWORD hitTarget = 0, DWORD tid = 0);
	static bool BreakpointDelete(DWORD64 breakpointOffset, DWORD breakpointType);

	static void RemoveSBPFromMemory(bool isDisable, DWORD processID);
	static void BreakpointInsertFromProjectFile(BPStruct newBreakpoint, int bpType);

	static QString GetBPComment(quint64 offset);
	static clsBreakpointManager* GetInstance();

signals:
	void OnBreakpointAdded(BPStruct newBreakpoint, int breakpointType);
	void OnBreakpointDeleted(quint64 bpOffset);

private:
	static clsBreakpointManager *s_instance;

	void BreakpointRebase(BPStruct *pCurrentBP, int bpType, HANDLE processHandle, DWORD processID);
	// Lock-free remove used internally by BreakpointClear/BreakpointRemove (caller holds write lock).
	void BreakpointRemoveImpl(DWORD64 breakpointOffset, DWORD breakpointType);
};

#endif