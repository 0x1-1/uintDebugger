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
#ifndef CLSCOMMANDPARSER_H
#define CLSCOMMANDPARSER_H

#include <QString>

/*
 * Command syntax (case-insensitive):
 *
 *  bp  <addr>              — software BP (INT3) at address
 *  bph <addr>              — hardware BP (exec) at address
 *  bpm <addr>              — memory BP (access) at address
 *  bpc <addr> <cond>       — conditional software BP (e.g. bpc 0x401000 eax==5)
 *  bd  <addr>              — delete BP at address
 *  bc                      — clear all breakpoints
 *  g                       — resume (continue) debugging
 *  t                       — step in (single-step)
 *  p                       — step over
 *  r                       — dump registers to log
 *  db  <addr>              — hex dump 128 bytes at address to log
 *  eval <expr>             — evaluate expression, print result to log
 *  w   <expr>              — add expression to watch window
 *  x   <module>            — print module base address to log
 *
 *  Addresses may be hex (0x prefix optional) or module::export notation.
 */
class clsCommandParser
{
public:
    /* Execute a command string.  Returns a result/error string for the log. */
    static QString Execute(const QString &cmd);

private:
    static quint64 ResolveAddress(const QString &token, bool &ok);
};

#endif // CLSCOMMANDPARSER_H
