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

#include <QString>
#include <QFont>
#include <QTableWidget>

enum class UiTheme { Dark, Light };

class clsThemeManager
{
public:
    // Apply theme to the running QApplication.
    // Reads font/theme from clsAppSettings::SharedInstance().
    static void apply();

    // Returns the full QSS string for the given theme + monospace font.
    static QString styleSheet(UiTheme theme,
                              const QString &monoFamily,
                              int monoSizePt);

    // Per-state status-bar QSS (colors adapt to current theme).
    static QString statusBarStyle(int stateType, UiTheme theme);

    // Set the uniform monospace font + DPI-aware row height on a table.
    // Call this in every table-containing widget constructor.
    static void applyTableFont(QTableWidget *table);

    // The currently active theme (updated by apply()).
    static UiTheme current();

private:
    static UiTheme s_current;
};
