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
#include "clsAntiAntiDebug.h"

#include <Psapi.h>

// ============================================================================
// Patch byte sequences
// ============================================================================
//
// All offsets verified by hand.  Key:
//   jne / jz use short (8-bit) relative displacements: displacement is signed
//   byte, target = (PC after instruction) + displacement.
//
// x64 — IsDebuggerPresent (no args):
//   xor eax,eax   31 C0
//   ret           C3
//   total: 3 bytes
//
// x64 — CheckRemoteDebuggerPresent (rcx=hProc, rdx=pbDebuggerPresent):
//   test rdx,rdx           48 85 D2           offset 0
//   jz   +3 → offset 8    74 03              offset 3
//   mov  byte[rdx],0       C6 02 00           offset 5
//   mov  eax,1             B8 01 00 00 00     offset 8   ← jz target
//   ret                    C3                 offset 13
//   total: 14 bytes
//
// x86 — CheckRemoteDebuggerPresent (stdcall, [esp+8]=pbDebuggerPresent):
//   mov eax,[esp+8]        8B 44 24 08        offset 0
//   test eax,eax           85 C0              offset 4
//   jz  +3 → offset 11    74 03              offset 6
//   mov byte[eax],0        C6 00 00           offset 8
//   mov eax,1              B8 01 00 00 00     offset 11  ← jz target
//   ret 8                  C2 08 00           offset 16
//   total: 19 bytes
//
// x64 — NtQueryInformationProcess conditional (class==7 → ProcessDebugPort):
//   cmp edx,7              83 FA 07           offset  0
//   jne +17 → offset 22   75 11              offset  3
//   test r8,r8             4D 85 C0           offset  5
//   jz  +9 → offset 19    74 09              offset  8
//   xor eax,eax            31 C0              offset 10
//   mov qword[r8],0        49 C7 00 00000000  offset 12  (7 bytes)
//   xor eax,eax            31 C0              offset 19  ← jz target
//   ret                    C3                 offset 21
//   [original @ offset 22] ← jne target
//   total: 22 bytes
//
// x86 — NtQueryInformationProcess conditional (class==7 → ProcessDebugPort):
//   mov edx,[esp+8]        8B 54 24 08        offset  0
//   cmp edx,7              83 FA 07           offset  4
//   jne +17 → offset 26   75 11              offset  7
//   mov edx,[esp+12]       8B 54 24 0C        offset  9
//   test edx,edx           85 D2              offset 13
//   jz  +4 → offset 21    74 04              offset 15
//   xor eax,eax            31 C0              offset 17
//   mov [edx],eax          89 02              offset 19
//   xor eax,eax            31 C0              offset 21  ← jz target
//   ret 20                 C2 14 00           offset 23
//   [original @ offset 26] ← jne target
//   total: 26 bytes
//
// x64 — NtSetInformationThread conditional (class==0x11 → ThreadHideFromDebugger):
//   cmp edx,0x11           83 FA 11           offset 0
//   jne +3 → offset 8     75 03              offset 3
//   xor eax,eax            31 C0              offset 5
//   ret                    C3                 offset 7
//   [original @ offset 8]  ← jne target
//   total: 8 bytes
//
// x86 — NtSetInformationThread conditional (class==0x11 → ThreadHideFromDebugger):
//   mov edx,[esp+8]        8B 54 24 08        offset  0
//   cmp edx,0x11           83 FA 11           offset  4
//   jne +5 → offset 14    75 05              offset  7
//   xor eax,eax            31 C0              offset  9
//   ret 16                 C2 10 00           offset 11
//   [original @ offset 14] ← jne target
//   total: 14 bytes

namespace {

// x64 IsDebuggerPresent / CheckRemoteDebuggerPresent etc. share this tiny stub.
static const quint8 kPatchIsDbgPres[] = {
    0x31, 0xC0,  // xor eax, eax
    0xC3         // ret
};

static const quint8 kPatchCheckRemoteX64[] = {
    0x48, 0x85, 0xD2,              // test rdx, rdx
    0x74, 0x03,                    // jz +3  → offset 8
    0xC6, 0x02, 0x00,              // mov byte ptr [rdx], 0
    0xB8, 0x01, 0x00, 0x00, 0x00,  // mov eax, 1
    0xC3                           // ret
};

static const quint8 kPatchCheckRemoteX86[] = {
    0x8B, 0x44, 0x24, 0x08,        // mov eax, [esp+8]
    0x85, 0xC0,                    // test eax, eax
    0x74, 0x03,                    // jz +3  → offset 11
    0xC6, 0x00, 0x00,              // mov byte ptr [eax], 0
    0xB8, 0x01, 0x00, 0x00, 0x00,  // mov eax, 1
    0xC2, 0x08, 0x00               // ret 8
};

static const quint8 kPatchNtQIPX64[] = {
    0x83, 0xFA, 0x07,              // cmp edx, 7
    0x75, 0x11,                    // jne +17  → offset 22 (original)
    0x4D, 0x85, 0xC0,              // test r8, r8
    0x74, 0x09,                    // jz  +9   → offset 19
    0x31, 0xC0,                    // xor eax, eax
    0x49, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov qword ptr [r8], 0
    0x31, 0xC0,                    // xor eax, eax   ← jz target (offset 19)
    0xC3                           // ret             (offset 21)
    // original bytes at offset 22
};

static const quint8 kPatchNtQIPX86[] = {
    0x8B, 0x54, 0x24, 0x08,        // mov edx, [esp+8]   (ProcessInformationClass)
    0x83, 0xFA, 0x07,              // cmp edx, 7
    0x75, 0x11,                    // jne +17 → offset 26 (original)
    0x8B, 0x54, 0x24, 0x0C,        // mov edx, [esp+12]  (ProcessInformation)
    0x85, 0xD2,                    // test edx, edx
    0x74, 0x04,                    // jz  +4  → offset 21
    0x31, 0xC0,                    // xor eax, eax
    0x89, 0x02,                    // mov dword ptr [edx], eax
    0x31, 0xC0,                    // xor eax, eax  ← jz target (offset 21)
    0xC2, 0x14, 0x00               // ret 20
    // original bytes at offset 26
};

static const quint8 kPatchNtSITX64[] = {
    0x83, 0xFA, 0x11,              // cmp edx, 0x11  (ThreadHideFromDebugger)
    0x75, 0x03,                    // jne +3 → offset 8 (original)
    0x31, 0xC0,                    // xor eax, eax
    0xC3                           // ret
    // original bytes at offset 8
};

static const quint8 kPatchNtSITX86[] = {
    0x8B, 0x54, 0x24, 0x08,        // mov edx, [esp+8]  (ThreadInformationClass)
    0x83, 0xFA, 0x11,              // cmp edx, 0x11
    0x75, 0x05,                    // jne +5 → offset 14 (original)
    0x31, 0xC0,                    // xor eax, eax
    0xC2, 0x10, 0x00               // ret 16
    // original bytes at offset 14
};

} // anonymous namespace

// ============================================================================
// Helper: apply / revert a single patch entry
// ============================================================================

bool clsAntiAntiDebug::patchApply(AntiAntiPatchEntry &entry)
{
    if (entry.applied || entry.address == 0 || m_hProc == nullptr)
        return false;

    const int len = entry.patchBytes.size();
    entry.originalBytes.resize(len);

    SIZE_T transferred = 0;
    if (!ReadProcessMemory(m_hProc, reinterpret_cast<LPCVOID>(entry.address),
                            entry.originalBytes.data(), len, &transferred)
        || static_cast<int>(transferred) < len)
    {
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtectEx(m_hProc, reinterpret_cast<LPVOID>(entry.address),
                           len, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        return false;
    }

    bool ok = WriteProcessMemory(m_hProc, reinterpret_cast<LPVOID>(entry.address),
                                  entry.patchBytes.constData(), len, &transferred)
              && static_cast<int>(transferred) == len;

    VirtualProtectEx(m_hProc, reinterpret_cast<LPVOID>(entry.address),
                     len, oldProtect, &oldProtect);

    if (ok)
        entry.applied = true;

    return ok;
}

bool clsAntiAntiDebug::patchRevert(AntiAntiPatchEntry &entry)
{
    if (!entry.applied || entry.address == 0 || m_hProc == nullptr)
        return false;

    const int len = entry.originalBytes.size();
    DWORD oldProtect = 0;
    VirtualProtectEx(m_hProc, reinterpret_cast<LPVOID>(entry.address),
                     len, PAGE_EXECUTE_READWRITE, &oldProtect);

    SIZE_T transferred = 0;
    bool ok = WriteProcessMemory(m_hProc, reinterpret_cast<LPVOID>(entry.address),
                                  entry.originalBytes.constData(), len, &transferred)
              && static_cast<int>(transferred) == len;

    VirtualProtectEx(m_hProc, reinterpret_cast<LPVOID>(entry.address),
                     len, oldProtect, &oldProtect);

    if (ok)
        entry.applied = false;

    return ok;
}

// ============================================================================
// Export resolution
// ============================================================================

DWORD64 clsAntiAntiDebug::resolveExportInProc(HANDLE hProc, bool x64,
                                               const char *module, const char *func)
{
    if (x64)
    {
        // All x64 processes share the same system-DLL base within a boot session.
        HMODULE hMod = GetModuleHandleA(module);
        if (!hMod)
            return 0;
        return reinterpret_cast<DWORD64>(GetProcAddress(hMod, func));
    }
    return resolve32BitExport(hProc, module, func);
}

DWORD64 clsAntiAntiDebug::resolve32BitExport(HANDLE hProc,
                                              const char *module, const char *func)
{
    // Enumerate 32-bit modules loaded in the (WOW64) target process.
    HMODULE mods[512] = {};
    DWORD needed = 0;
    if (!EnumProcessModulesEx(hProc, mods, sizeof(mods), &needed, LIST_MODULES_32BIT))
        return 0;

    const int count = static_cast<int>(needed / sizeof(HMODULE));
    char modName[MAX_PATH];
    for (int i = 0; i < count; ++i)
    {
        if (GetModuleBaseNameA(hProc, mods[i], modName, MAX_PATH) == 0)
            continue;
        if (_stricmp(modName, module) != 0)
            continue;

        const DWORD64 base = reinterpret_cast<DWORD64>(mods[i]);

        // --- parse PE export table from the target process ---
        SIZE_T rd = 0;

        IMAGE_DOS_HEADER dosHdr;
        if (!ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(base),
                                &dosHdr, sizeof(dosHdr), &rd) || rd < sizeof(dosHdr))
            return 0;
        if (dosHdr.e_magic != IMAGE_DOS_SIGNATURE)
            return 0;

        IMAGE_NT_HEADERS32 ntHdrs;
        if (!ReadProcessMemory(hProc,
                                reinterpret_cast<LPCVOID>(base + dosHdr.e_lfanew),
                                &ntHdrs, sizeof(ntHdrs), &rd) || rd < sizeof(ntHdrs))
            return 0;

        const DWORD expRVA  = ntHdrs.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        const DWORD expSize = ntHdrs.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        if (expRVA == 0 || expSize == 0)
            return 0;

        IMAGE_EXPORT_DIRECTORY expDir;
        if (!ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(base + expRVA),
                                &expDir, sizeof(expDir), &rd) || rd < sizeof(expDir))
            return 0;

        const int numNames = static_cast<int>(expDir.NumberOfNames);
        QVector<DWORD> nameRVAs(numNames);
        QVector<WORD>  nameOrds(numNames);
        QVector<DWORD> funcRVAs(static_cast<int>(expDir.NumberOfFunctions));

        if (!ReadProcessMemory(hProc,
                                reinterpret_cast<LPCVOID>(base + expDir.AddressOfNames),
                                nameRVAs.data(), numNames * sizeof(DWORD), &rd))
            return 0;
        if (!ReadProcessMemory(hProc,
                                reinterpret_cast<LPCVOID>(base + expDir.AddressOfNameOrdinals),
                                nameOrds.data(), numNames * sizeof(WORD), &rd))
            return 0;
        if (!ReadProcessMemory(hProc,
                                reinterpret_cast<LPCVOID>(base + expDir.AddressOfFunctions),
                                funcRVAs.data(),
                                expDir.NumberOfFunctions * sizeof(DWORD), &rd))
            return 0;

        for (int j = 0; j < numNames; ++j)
        {
            char name[256] = {};
            if (!ReadProcessMemory(hProc,
                                    reinterpret_cast<LPCVOID>(base + nameRVAs[j]),
                                    name, sizeof(name) - 1, &rd))
                continue;
            if (strcmp(name, func) != 0)
                continue;

            const WORD ordIdx = nameOrds[j];
            if (ordIdx >= expDir.NumberOfFunctions)
                return 0;
            const DWORD funcRVA = funcRVAs[ordIdx];
            if (funcRVA == 0)
                return 0;
            return base + funcRVA;
        }
        return 0; // function not found in this module
    }
    return 0; // module not found
}

// ============================================================================
// Public API
// ============================================================================

clsAntiAntiDebug::clsAntiAntiDebug()
{
}

void clsAntiAntiDebug::setup(HANDLE hProc, bool x64)
{
    reset();
    m_hProc = hProc;
    m_x64   = x64;
}

void clsAntiAntiDebug::reset()
{
    m_patches.clear();
    m_hProc = nullptr;
    m_x64   = false;
}

bool clsAntiAntiDebug::ApplyAll()
{
    bool ok = true;
    ok &= PatchIsDebuggerPresent();
    ok &= PatchCheckRemoteDebuggerPresent();
    ok &= PatchNtQueryInformationProcess();
    ok &= PatchNtSetInformationThread();
    return ok;
}

bool clsAntiAntiDebug::UnpatchAll()
{
    bool ok = true;
    for (AntiAntiPatchEntry &e : m_patches)
        ok &= patchRevert(e);
    return ok;
}

bool clsAntiAntiDebug::PatchIsDebuggerPresent()
{
    // Check if already patched.
    for (AntiAntiPatchEntry &e : m_patches)
        if (e.name == "IsDebuggerPresent")
            return e.applied; // already applied, nothing to do

    AntiAntiPatchEntry entry;
    entry.name    = "IsDebuggerPresent";
    entry.address = resolveExportInProc(m_hProc, m_x64,
                                         "kernel32.dll", "IsDebuggerPresent");
    entry.patchBytes = QByteArray(reinterpret_cast<const char *>(kPatchIsDbgPres),
                                   sizeof(kPatchIsDbgPres));
    m_patches.append(entry);
    return patchApply(m_patches.last());
}

bool clsAntiAntiDebug::PatchCheckRemoteDebuggerPresent()
{
    for (AntiAntiPatchEntry &e : m_patches)
        if (e.name == "CheckRemoteDebuggerPresent")
            return e.applied;

    AntiAntiPatchEntry entry;
    entry.name    = "CheckRemoteDebuggerPresent";
    entry.address = resolveExportInProc(m_hProc, m_x64,
                                         "kernel32.dll", "CheckRemoteDebuggerPresent");
    if (m_x64)
    {
        entry.patchBytes = QByteArray(
            reinterpret_cast<const char *>(kPatchCheckRemoteX64),
            sizeof(kPatchCheckRemoteX64));
    }
    else
    {
        entry.patchBytes = QByteArray(
            reinterpret_cast<const char *>(kPatchCheckRemoteX86),
            sizeof(kPatchCheckRemoteX86));
    }
    m_patches.append(entry);
    return patchApply(m_patches.last());
}

bool clsAntiAntiDebug::PatchNtQueryInformationProcess()
{
    for (AntiAntiPatchEntry &e : m_patches)
        if (e.name == "NtQueryInformationProcess")
            return e.applied;

    AntiAntiPatchEntry entry;
    entry.name    = "NtQueryInformationProcess";
    entry.address = resolveExportInProc(m_hProc, m_x64,
                                         "ntdll.dll", "NtQueryInformationProcess");
    if (m_x64)
    {
        entry.patchBytes = QByteArray(
            reinterpret_cast<const char *>(kPatchNtQIPX64),
            sizeof(kPatchNtQIPX64));
    }
    else
    {
        entry.patchBytes = QByteArray(
            reinterpret_cast<const char *>(kPatchNtQIPX86),
            sizeof(kPatchNtQIPX86));
    }
    m_patches.append(entry);
    return patchApply(m_patches.last());
}

bool clsAntiAntiDebug::PatchNtSetInformationThread()
{
    for (AntiAntiPatchEntry &e : m_patches)
        if (e.name == "NtSetInformationThread")
            return e.applied;

    AntiAntiPatchEntry entry;
    entry.name    = "NtSetInformationThread";
    entry.address = resolveExportInProc(m_hProc, m_x64,
                                         "ntdll.dll", "NtSetInformationThread");
    if (m_x64)
    {
        entry.patchBytes = QByteArray(
            reinterpret_cast<const char *>(kPatchNtSITX64),
            sizeof(kPatchNtSITX64));
    }
    else
    {
        entry.patchBytes = QByteArray(
            reinterpret_cast<const char *>(kPatchNtSITX86),
            sizeof(kPatchNtSITX86));
    }
    m_patches.append(entry);
    return patchApply(m_patches.last());
}
