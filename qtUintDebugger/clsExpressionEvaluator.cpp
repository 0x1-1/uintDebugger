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
#include "clsExpressionEvaluator.h"

#include <QStringList>

namespace
{

quint64 readReg32(const QString &name, const WOW64_CONTEXT *ctx, bool &found)
{
    found = true;
    const QString n = name.toLower();
    if(n == "eax")    return ctx->Eax;
    if(n == "ebx")    return ctx->Ebx;
    if(n == "ecx")    return ctx->Ecx;
    if(n == "edx")    return ctx->Edx;
    if(n == "esi")    return ctx->Esi;
    if(n == "edi")    return ctx->Edi;
    if(n == "esp")    return ctx->Esp;
    if(n == "ebp")    return ctx->Ebp;
    if(n == "eip")    return ctx->Eip;
    if(n == "eflags") return ctx->EFlags;
    found = false;
    return 0;
}

#ifdef _AMD64_
quint64 readReg64(const QString &name, const CONTEXT *ctx, bool &found)
{
    found = true;
    const QString n = name.toLower();
    if(n == "rax")    return ctx->Rax;
    if(n == "rbx")    return ctx->Rbx;
    if(n == "rcx")    return ctx->Rcx;
    if(n == "rdx")    return ctx->Rdx;
    if(n == "rsi")    return ctx->Rsi;
    if(n == "rdi")    return ctx->Rdi;
    if(n == "rsp")    return ctx->Rsp;
    if(n == "rbp")    return ctx->Rbp;
    if(n == "rip")    return ctx->Rip;
    if(n == "r8")     return ctx->R8;
    if(n == "r9")     return ctx->R9;
    if(n == "r10")    return ctx->R10;
    if(n == "r11")    return ctx->R11;
    if(n == "r12")    return ctx->R12;
    if(n == "r13")    return ctx->R13;
    if(n == "r14")    return ctx->R14;
    if(n == "r15")    return ctx->R15;
    // also accept 32-bit names in 64-bit mode
    if(n == "eax")    return (DWORD)ctx->Rax;
    if(n == "ebx")    return (DWORD)ctx->Rbx;
    if(n == "ecx")    return (DWORD)ctx->Rcx;
    if(n == "edx")    return (DWORD)ctx->Rdx;
    if(n == "esi")    return (DWORD)ctx->Rsi;
    if(n == "edi")    return (DWORD)ctx->Rdi;
    if(n == "esp")    return (DWORD)ctx->Rsp;
    if(n == "ebp")    return (DWORD)ctx->Rbp;
    if(n == "eip")    return (DWORD)ctx->Rip;
    if(n == "eflags") return ctx->EFlags;
    found = false;
    return 0;
}
#endif

} // namespace

quint64 clsExpressionEvaluator::evaluate(const QString &expr, HANDLE hProcess,
                                          const void *pContext, bool isWow64,
                                          bool *ok)
{
    auto setOk = [&](bool v) { if(ok) *ok = v; };

    const QString e = expr.trimmed();
    if(e.isEmpty())
    {
        setOk(false);
        return 0;
    }

    // --- Memory dereference: [inner] ---
    if(e.startsWith('[') && e.endsWith(']'))
    {
        const QString inner = e.mid(1, e.length() - 2).trimmed();
        bool innerOk = false;
        quint64 addr = evaluate(inner, hProcess, pContext, isWow64, &innerOk);
        if(!innerOk)
        {
            setOk(false);
            return 0;
        }
        DWORD val = 0;
        SIZE_T bytesRead = 0;
        if(!ReadProcessMemory(hProcess, (LPVOID)addr, &val, sizeof(val), &bytesRead) || bytesRead != sizeof(val))
        {
            setOk(false);
            return 0;
        }
        setOk(true);
        return (quint64)val;
    }

    // --- Binary +/- (find last operator not inside brackets) ---
    int depth = 0;
    int opPos = -1;
    QChar opChar = ' ';
    for(int i = e.length() - 1; i >= 0; i--)
    {
        const QChar c = e[i];
        if(c == ']') { depth++; continue; }
        if(c == '[') { depth--; continue; }
        if(depth != 0) continue;
        if((c == '+' || c == '-') && i > 0)
        {
            opPos = i;
            opChar = c;
            break;
        }
    }

    if(opPos > 0)
    {
        const QString left  = e.left(opPos).trimmed();
        const QString right = e.mid(opPos + 1).trimmed();
        bool lok = false, rok = false;
        quint64 lv = evaluate(left,  hProcess, pContext, isWow64, &lok);
        quint64 rv = evaluate(right, hProcess, pContext, isWow64, &rok);
        if(!lok || !rok) { setOk(false); return 0; }
        setOk(true);
        return opChar == '+' ? lv + rv : lv - rv;
    }

    // --- Register name ---
    bool regFound = false;
    quint64 regVal = 0;

#ifdef _AMD64_
    if(!isWow64 && pContext)
        regVal = readReg64(e, reinterpret_cast<const CONTEXT *>(pContext), regFound);
    else
#endif
    if(isWow64 && pContext)
        regVal = readReg32(e, reinterpret_cast<const WOW64_CONTEXT *>(pContext), regFound);

    if(regFound)
    {
        setOk(true);
        return regVal;
    }

    // --- Hex literal ---
    QString hexStr = e;
    if(hexStr.startsWith("0x", Qt::CaseInsensitive))
        hexStr = hexStr.mid(2);

    bool hexOk = false;
    quint64 hexVal = hexStr.toULongLong(&hexOk, 16);
    if(hexOk)
    {
        setOk(true);
        return hexVal;
    }

    // --- Decimal literal ---
    bool decOk = false;
    quint64 decVal = e.toULongLong(&decOk, 10);
    if(decOk)
    {
        setOk(true);
        return decVal;
    }

    setOk(false);
    return 0;
}

bool clsExpressionEvaluator::evaluateCondition(const QString &cond, HANDLE hProcess,
                                                const void *pContext, bool isWow64)
{
    // Try operators longest-first to avoid matching < when we have <=
    static const char *ops[] = { "==", "!=", "<=", ">=", "<", ">", nullptr };

    for(int i = 0; ops[i] != nullptr; i++)
    {
        const QString op = QString::fromLatin1(ops[i]);
        const int pos = cond.indexOf(op);
        if(pos < 0)
            continue;

        const QString lhs = cond.left(pos).trimmed();
        const QString rhs = cond.mid(pos + op.length()).trimmed();

        if(lhs.isEmpty() || rhs.isEmpty())
            continue;

        bool lok = false, rok = false;
        quint64 lv = evaluate(lhs, hProcess, pContext, isWow64, &lok);
        quint64 rv = evaluate(rhs, hProcess, pContext, isWow64, &rok);

        if(!lok || !rok)
            return true; // parse error: conservative, don't suppress

        if(op == "==") return lv == rv;
        if(op == "!=") return lv != rv;
        if(op == "<=") return lv <= rv;
        if(op == ">=") return lv >= rv;
        if(op == "<")  return lv <  rv;
        if(op == ">")  return lv >  rv;
    }

    return true; // no operator found: conservative
}
