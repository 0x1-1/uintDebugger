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
#include "clsCommandParser.h"

#include "clsBreakpointManager.h"
#include "clsHelperClass.h"
#include "clsAPIImport.h"
#include "clsMemManager.h"
#include "clsExpressionEvaluator.h"

#include "qtDLGUintDebugger.h"
#include "qtDLGWatch.h"

#include <QMetaObject>
#include <Windows.h>

// ---------------------------------------------------------------------------
// Resolve an address token: "0x1234", "1234" (hex), or "module::export".
// ---------------------------------------------------------------------------
quint64 clsCommandParser::ResolveAddress(const QString &token, bool &ok)
{
    ok = false;
    if(token.isEmpty())
        return 0;

    qtDLGUintDebugger *pMain = qtDLGUintDebugger::GetInstance();
    if(!pMain) return 0;

    if(token.contains("::"))
    {
        QStringList parts = token.split("::");
        if(parts.size() < 2) return 0;

        quint64 base = clsHelperClass::CalcOffsetForModule(
            (PTCHAR)parts[0].toLower().toStdWString().c_str(),
            NULL,
            pMain->coreDebugger->GetCurrentPID());

        quint64 addr = clsHelperClass::RemoteGetProcAddr(parts[1], base,
            pMain->coreDebugger->GetCurrentPID());

        if(addr == 0) return 0;
        ok = true;
        return addr;
    }

    // Plain hex (0x prefix optional)
    QString hex = token;
    if(hex.startsWith("0x", Qt::CaseInsensitive))
        hex = hex.mid(2);

    quint64 addr = hex.toULongLong(&ok, 16);
    return addr;
}

// ---------------------------------------------------------------------------
// Execute one command line. Returns a log string.
// ---------------------------------------------------------------------------
QString clsCommandParser::Execute(const QString &rawCmd)
{
    const QString cmd = rawCmd.trimmed();
    if(cmd.isEmpty())
        return QString();

    qtDLGUintDebugger *pMain = qtDLGUintDebugger::GetInstance();
    if(!pMain) return QStringLiteral("[cmd] ERROR: main window not available");

    clsDebugger  *dbg   = pMain->coreDebugger;
    const bool    isDbg = dbg->GetDebuggingState();

    // Split on first whitespace: verb + optional argument
    const int     sp   = cmd.indexOf(' ');
    const QString verb = (sp < 0 ? cmd : cmd.left(sp)).toLower();
    const QString arg  = (sp < 0 ? QString() : cmd.mid(sp + 1).trimmed());

    // -----------------------------------------------------------------------
    // Commands that don't need an active debugger session
    // -----------------------------------------------------------------------

    if(verb == "bc")
    {
        clsBreakpointManager *bpm = clsBreakpointManager::GetInstance();
        if(bpm) bpm->BreakpointClear();
        return QStringLiteral("[cmd] All breakpoints cleared.");
    }

    // -----------------------------------------------------------------------
    // Commands that require an active session
    // -----------------------------------------------------------------------

    if(!isDbg)
        return QStringLiteral("[cmd] ERROR: no active debugging session.");

    // --- g : resume --------------------------------------------------------
    if(verb == "g")
    {
        if(!dbg->ResumeDebugging())
            return QStringLiteral("[cmd] g: resume failed.");
        pMain->UpdateStateBar(STATE_RUN);
        return QStringLiteral("[cmd] Resumed.");
    }

    // --- t : step in -------------------------------------------------------
    if(verb == "t")
    {
        if(!dbg->StepIn())
            return QStringLiteral("[cmd] t: only valid while the debugger is at a break point.");
        return QStringLiteral("[cmd] Step in.");
    }

    // --- p : step over -----------------------------------------------------
    if(verb == "p")
    {
        if(!dbg->IsBreaking())
            return QStringLiteral("[cmd] p: only valid while the debugger is at a break point.");

        if(!QMetaObject::invokeMethod(
            pMain,
            "action_DebugStepOver",
            Qt::DirectConnection))
        {
            return QStringLiteral("[cmd] p: StepOver failed.");
        }

        return QStringLiteral("[cmd] Step over.");
    }

    // --- r : register dump -------------------------------------------------
    if(verb == "r")
    {
        // Context is only valid (and non-racy) while the debug thread is blocked.
        if(!dbg->IsBreaking())
            return QStringLiteral("[cmd] r: registers are only valid at a break point.");

        QString out;
#ifdef _AMD64_
        BOOL isWow = FALSE;
        if(clsAPIImport::pIsWow64Process)
            clsAPIImport::pIsWow64Process(dbg->GetCurrentProcessHandle(), &isWow);
        if(isWow)
        {
            out = QStringLiteral("[cmd] Registers (WOW64):\n"
                " EAX=%1  EBX=%2  ECX=%3  EDX=%4\n"
                " ESI=%5  EDI=%6  ESP=%7  EBP=%8\n"
                " EIP=%9  EFL=%10")
                .arg(dbg->wowProcessContext.Eax,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.Ebx,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.Ecx,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.Edx,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.Esi,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.Edi,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.Esp,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.Ebp,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.Eip,  8,16,QChar('0'))
                .arg(dbg->wowProcessContext.EFlags,8,16,QChar('0'));
        }
        else
        {
            out = QStringLiteral("[cmd] Registers (x64):\n"
                " RAX=%1  RBX=%2  RCX=%3  RDX=%4\n"
                " RSI=%5  RDI=%6  RSP=%7  RBP=%8\n"
                " RIP=%9  EFL=%10")
                .arg(dbg->ProcessContext.Rax, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.Rbx, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.Rcx, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.Rdx, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.Rsi, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.Rdi, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.Rsp, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.Rbp, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.Rip, 16,16,QChar('0'))
                .arg(dbg->ProcessContext.EFlags,8,16,QChar('0'));
        }
#else
        out = QStringLiteral("[cmd] Registers (x86):\n"
            " EAX=%1  EBX=%2  ECX=%3  EDX=%4\n"
            " ESI=%5  EDI=%6  ESP=%7  EBP=%8\n"
            " EIP=%9  EFL=%10")
            .arg(dbg->ProcessContext.Eax,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.Ebx,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.Ecx,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.Edx,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.Esi,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.Edi,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.Esp,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.Ebp,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.Eip,  8,16,QChar('0'))
            .arg(dbg->ProcessContext.EFlags,8,16,QChar('0'));
#endif
        return out;
    }

    // --- bp / bph / bpm / bd : breakpoints --------------------------------
    if(verb == "bp" || verb == "bph" || verb == "bpm" || verb == "bd")
    {
        if(arg.isEmpty())
            return QStringLiteral("[cmd] ERROR: address required.");

        bool ok = false;
        quint64 addr = ResolveAddress(arg, ok);
        if(!ok || addr == 0)
            return QStringLiteral("[cmd] ERROR: cannot resolve '%1'.").arg(arg);

        DWORD pid = dbg->GetCurrentPID();

        if(verb == "bp")
        {
            if(clsBreakpointManager::BreakpointInsert(SOFTWARE_BP, BP_EXEC, pid, addr, 1, BP_KEEP))
                return QStringLiteral("[cmd] SW BP set at 0x%1.").arg(addr,16,16,QChar('0'));
            return QStringLiteral("[cmd] ERROR: failed to set SW BP at 0x%1.").arg(addr,16,16,QChar('0'));
        }
        if(verb == "bph")
        {
            if(clsBreakpointManager::BreakpointInsert(HARDWARE_BP, BP_EXEC, pid, addr, 1, BP_KEEP))
                return QStringLiteral("[cmd] HW BP set at 0x%1.").arg(addr,16,16,QChar('0'));
            return QStringLiteral("[cmd] ERROR: failed to set HW BP at 0x%1.").arg(addr,16,16,QChar('0'));
        }
        if(verb == "bpm")
        {
            if(clsBreakpointManager::BreakpointInsert(MEMORY_BP, BP_ACCESS, pid, addr, 1, BP_KEEP))
                return QStringLiteral("[cmd] Mem BP set at 0x%1.").arg(addr,16,16,QChar('0'));
            return QStringLiteral("[cmd] ERROR: failed to set Mem BP at 0x%1.").arg(addr,16,16,QChar('0'));
        }
        // bd
        bool swDel  = clsBreakpointManager::BreakpointDelete(addr, SOFTWARE_BP);
        bool hwDel  = clsBreakpointManager::BreakpointDelete(addr, HARDWARE_BP);
        bool memDel = clsBreakpointManager::BreakpointDelete(addr, MEMORY_BP);
        if(swDel || hwDel || memDel)
            return QStringLiteral("[cmd] BP deleted at 0x%1.").arg(addr,16,16,QChar('0'));
        return QStringLiteral("[cmd] No BP found at 0x%1.").arg(addr,16,16,QChar('0'));
    }

    // --- db : hex dump 128 bytes -------------------------------------------
    if(verb == "db")
    {
        if(arg.isEmpty())
            return QStringLiteral("[cmd] ERROR: address required.");

        bool ok = false;
        quint64 addr = ResolveAddress(arg, ok);
        if(!ok || addr == 0)
            return QStringLiteral("[cmd] ERROR: cannot resolve '%1'.").arg(arg);

        HANDLE hProc = dbg->GetCurrentProcessHandle();
        if(hProc == NULL || hProc == INVALID_HANDLE_VALUE)
            return QStringLiteral("[cmd] ERROR: no process handle.");

        constexpr int kBytes = 128;
        BYTE buf[kBytes] = {};
        SIZE_T read = 0;
        if(!ReadProcessMemory(hProc, (LPCVOID)addr, buf, kBytes, &read) || read == 0)
            return QStringLiteral("[cmd] ERROR: ReadProcessMemory failed at 0x%1.").arg(addr,16,16,QChar('0'));

        QString out = QStringLiteral("[cmd] db 0x%1:\n").arg(addr,16,16,QChar('0'));
        for(int i = 0; i < (int)read; i += 16)
        {
            out += QStringLiteral("  %1  ").arg(addr + i, 16, 16, QChar('0'));
            QString ascii;
            for(int j = i; j < i + 16 && j < (int)read; ++j)
            {
                out   += QStringLiteral("%1 ").arg(buf[j], 2, 16, QChar('0'));
                ascii += (buf[j] >= 0x20 && buf[j] < 0x7F) ? QChar(buf[j]) : QChar('.');
            }
            // pad last row
            int shown = qMin(16, (int)read - i);
            for(int k = shown; k < 16; ++k) out += QStringLiteral("   ");
            out += QStringLiteral(" %1\n").arg(ascii);
        }
        return out;
    }

    // --- eval : evaluate expression ----------------------------------------
    if(verb == "eval")
    {
        if(arg.isEmpty())
            return QStringLiteral("[cmd] eval: expression required.");

        HANDLE hProc = dbg->GetCurrentProcessHandle();
        bool isWow64 = false;
#ifdef _AMD64_
        BOOL bWow = false;
        if(clsAPIImport::pIsWow64Process)
            clsAPIImport::pIsWow64Process(hProc, &bWow);
        isWow64 = (bWow != 0);
#endif
        const void *pCtx = isWow64
            ? static_cast<const void *>(&dbg->wowProcessContext)
            : static_cast<const void *>(&dbg->ProcessContext);

        bool ok = false;
        quint64 val = clsExpressionEvaluator::evaluate(arg, hProc, pCtx, isWow64, &ok);
        if(!ok)
            return QStringLiteral("[eval] %1 = <parse error>").arg(arg);
        return QStringLiteral("[eval] %1 = 0x%2 (%3)").arg(arg)
            .arg(val, 0, 16)
            .arg(val);
    }

    // --- bpc : conditional software BP ------------------------------------
    if(verb == "bpc")
    {
        // Syntax: bpc <addr> <condition>
        const int condSep = arg.indexOf(' ');
        if(condSep < 0)
            return QStringLiteral("[cmd] bpc: usage: bpc <addr> <condition>");

        const QString addrToken = arg.left(condSep).trimmed();
        const QString condition = arg.mid(condSep + 1).trimmed();

        bool ok = false;
        quint64 addr = ResolveAddress(addrToken, ok);
        if(!ok || addr == 0)
            return QStringLiteral("[cmd] bpc: cannot resolve '%1'.").arg(addrToken);

        DWORD pid = dbg->GetCurrentPID();
        if(!clsBreakpointManager::BreakpointInsert(SOFTWARE_BP, BP_EXEC, pid, addr, 1, BP_KEEP))
            return QStringLiteral("[cmd] bpc: failed to set BP at 0x%1.").arg(addr,16,16,QChar('0'));

        clsBreakpointManager *bpm = clsBreakpointManager::GetInstance();
        if(bpm && !condition.isEmpty())
            bpm->BreakpointSetCondition(addr, SOFTWARE_BP, condition);

        return QStringLiteral("[cmd] Conditional SW BP at 0x%1  cond: %2")
            .arg(addr, 16, 16, QChar('0')).arg(condition);
    }

    // --- w : add watch expression ------------------------------------------
    if(verb == "w")
    {
        if(arg.isEmpty())
            return QStringLiteral("[cmd] w: expression required.");
        if(pMain->dlgWatch)
        {
            pMain->dlgWatch->AddExpression(arg);
            return QStringLiteral("[watch] Added: %1").arg(arg);
        }
        return QStringLiteral("[cmd] w: watch window not available.");
    }

    // --- x : show module base ----------------------------------------------
    if(verb == "x")
    {
        if(arg.isEmpty())
            return QStringLiteral("[cmd] x: module name required.");

        DWORD pid = dbg->GetCurrentPID();
        quint64 base = clsHelperClass::CalcOffsetForModule(
            (PTCHAR)arg.toLower().toStdWString().c_str(), NULL, pid);

        if(base == 0)
            return QStringLiteral("[cmd] x: module '%1' not found.").arg(arg);

        return QStringLiteral("[x] %1  base=0x%2").arg(arg).arg(base, 16, 16, QChar('0'));
    }

    // --- help --------------------------------------------------------------
    if(verb == "?" || verb == "help")
    {
        return QStringLiteral(
            "[cmd] Commands:\n"
            "  bp  <addr>          - software BP (INT3)\n"
            "  bph <addr>          - hardware BP (exec)\n"
            "  bpm <addr>          - memory BP (access)\n"
            "  bpc <addr> <cond>   - conditional software BP\n"
            "  bd  <addr>          - delete BP at address\n"
            "  bc                  - clear all BPs\n"
            "  g                   - resume\n"
            "  t                   - step in (requires break)\n"
            "  p                   - step over (requires break)\n"
            "  r                   - dump registers (requires break)\n"
            "  db  <addr>          - hex dump 128 bytes\n"
            "  eval <expr>         - evaluate expression\n"
            "  w   <expr>          - add to watch window\n"
            "  x   <module>        - show module base address\n"
            "  <addr>: hex (0x optional) or module::export");
    }

    return QStringLiteral("[cmd] Unknown command '%1'. Type '?' for help.").arg(verb);
}
