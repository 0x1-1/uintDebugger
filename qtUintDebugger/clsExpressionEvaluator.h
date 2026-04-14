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
#pragma once

#include <Windows.h>
#include <QString>

// Lightweight expression evaluator for conditional breakpoints and watch window.
//
// Supported syntax for evaluate():
//   register names : eax ebx ecx edx esi edi esp ebp eip eflags (x86/WOW64)
//                    rax rbx rcx rdx rsi rdi rsp rbp rip r8..r15 (x64 native)
//   hex literal    : 0x1A2B or 1A2B (bare hex without 0x)
//   decimal literal: 12345
//   memory deref   : [expr]  — reads 4 bytes at the evaluated address
//   arithmetic     : expr + expr  |  expr - expr
//
// Supported syntax for evaluateCondition():
//   LHS op RHS  where op is one of:  ==  !=  <=  >=  <  >
//
// On parse error evaluate() returns 0 and sets *ok = false.
// evaluateCondition() returns true on parse error (conservative — don't suppress the BP).

class clsExpressionEvaluator
{
public:
    static quint64 evaluate(const QString &expr, HANDLE hProcess,
                            const void *pContext, bool isWow64,
                            bool *ok = nullptr);

    static bool evaluateCondition(const QString &cond, HANDLE hProcess,
                                  const void *pContext, bool isWow64);
};
