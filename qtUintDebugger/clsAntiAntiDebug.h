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
#ifndef CLSANTIANTIDEBUG_H
#define CLSANTIANTIDEBUG_H

#include <Windows.h>
#include <QByteArray>
#include <QString>
#include <QVector>

// ---------------------------------------------------------------------------
// Per-patch descriptor: holds original bytes for safe revert.
// ---------------------------------------------------------------------------
struct AntiAntiPatchEntry
{
    QString    name;
    DWORD64    address      {0};
    QByteArray originalBytes;   // filled on first Apply
    QByteArray patchBytes;      // the bytes we write
    bool       applied      {false};
};

// ---------------------------------------------------------------------------
// clsAntiAntiDebug
//
// Patches four common anti-debug APIs in the target process so they always
// report "not being debugged":
//   - kernel32!IsDebuggerPresent               → xor eax,eax ; ret
//   - kernel32!CheckRemoteDebuggerPresent       → zero *pbDebuggerPresent, ret TRUE
//   - ntdll!NtQueryInformationProcess           → conditional: if class==7, return 0
//   - ntdll!NtSetInformationThread              → conditional: if class==0x11, ret STATUS_SUCCESS
//
// Usage:
//   setup(hProcess, isX64);   // called after CREATE_PROCESS_DEBUG_EVENT
//   ApplyAll();               // apply all patches
//   UnpatchAll();             // restore originals (call before process handle close)
//   reset();                  // clear state (called when debug session ends)
// ---------------------------------------------------------------------------
class clsAntiAntiDebug
{
public:
    clsAntiAntiDebug();

    // Configure for a target process.  Clears previous state.
    void setup(HANDLE hProc, bool x64);

    // Drop all patch state (after process exits or when we can no longer patch).
    void reset();

    // Apply/revert all four patches.  Return true if every patch succeeded.
    bool ApplyAll();
    bool UnpatchAll();

    // Individual patches (idempotent: second call while already applied is a no-op).
    bool PatchIsDebuggerPresent();
    bool PatchCheckRemoteDebuggerPresent();
    bool PatchNtQueryInformationProcess();
    bool PatchNtSetInformationThread();

    const QVector<AntiAntiPatchEntry>& patches() const { return m_patches; }

    bool isSetup() const { return m_hProc != nullptr; }

private:
    HANDLE  m_hProc {nullptr};
    bool    m_x64   {false};
    QVector<AntiAntiPatchEntry> m_patches;

    bool patchApply(AntiAntiPatchEntry &entry);
    bool patchRevert(AntiAntiPatchEntry &entry);

    // Resolve function address in the target process.
    // x64 target: use GetModuleHandleA + GetProcAddress (same ASLR base).
    // x86 target: enumerate 32-bit modules + manual PE export parse.
    static DWORD64 resolveExportInProc(HANDLE hProc, bool x64,
                                        const char *module, const char *func);
    static DWORD64 resolve32BitExport(HANDLE hProc,
                                       const char *module, const char *func);
};

#endif // CLSANTIANTIDEBUG_H
