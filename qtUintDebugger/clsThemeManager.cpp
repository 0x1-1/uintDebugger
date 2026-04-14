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
#include "clsThemeManager.h"
#include "clsAppSettings.h"

#include <QApplication>
#include <QFontMetrics>
#include <QHeaderView>

UiTheme clsThemeManager::s_current = UiTheme::Dark;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

UiTheme clsThemeManager::current()
{
    return s_current;
}

void clsThemeManager::apply()
{
    clsAppSettings *cfg = clsAppSettings::SharedInstance();
    const UiTheme t       = cfg->isDarkTheme() ? UiTheme::Dark : UiTheme::Light;
    const QString family  = cfg->monospaceFontFamily();
    const int     sizePt  = cfg->monospaceFontSize();

    s_current = t;
    qApp->setStyleSheet(styleSheet(t, family, sizePt));
}

void clsThemeManager::applyTableFont(QTableWidget *table)
{
    if(!table) return;

    clsAppSettings *cfg = clsAppSettings::SharedInstance();
    QFont f(cfg->monospaceFontFamily(), cfg->monospaceFontSize());
    table->setFont(f);

    const int rowH = QFontMetrics(f).height() + 4;
    table->verticalHeader()->setDefaultSectionSize(rowH);
    table->verticalHeader()->setMinimumSectionSize(rowH);
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

QString clsThemeManager::statusBarStyle(int stateType, UiTheme theme)
{
    // stateType: 1=RUN(green), 2=SUSPEND(yellow), 3=ERROR(red), else=IDLE(blue)
    // Colours are slightly muted in light mode.
    struct StateColors { const char *bg, *fg; };

    static const StateColors dark[4] = {
        { "#123525", "#ecfff1" }, // RUN
        { "#5d4700", "#fff5cc" }, // SUSPEND
        { "#5a1d1d", "#ffe1e1" }, // ERROR
        { "#163a5f", "#eaf4ff" }, // IDLE
    };
    static const StateColors light[4] = {
        { "#d4edda", "#155724" }, // RUN
        { "#fff3cd", "#856404" }, // SUSPEND
        { "#f8d7da", "#721c24" }, // ERROR
        { "#cce5ff", "#004085" }, // IDLE
    };

    int idx = 3; // default: IDLE
    if(stateType == 1) idx = 0;
    else if(stateType == 2) idx = 1;
    else if(stateType == 3) idx = 2;

    const StateColors &c = (theme == UiTheme::Dark) ? dark[idx] : light[idx];
    return QString("QStatusBar { background-color: %1; color: %2; font-weight: 600; }")
        .arg(c.bg).arg(c.fg);
}

// ---------------------------------------------------------------------------
// Full stylesheet generator
// ---------------------------------------------------------------------------

QString clsThemeManager::styleSheet(UiTheme theme,
                                    const QString &monoFamily,
                                    int monoSizePt)
{
    // ------------------------------------------------------------------
    // Colour tokens
    // ------------------------------------------------------------------
    struct Tokens {
        const char *bg0;        // widget base
        const char *bg1;        // slightly raised surface
        const char *bg2;        // hover / secondary
        const char *bgInput;    // line edits, spin boxes
        const char *bgTable;    // table background
        const char *bgTableAlt; // alternating row
        const char *bgHeader;   // table header
        const char *bgDock;     // dock title
        const char *bgMenu;     // menus
        const char *bgTab;      // inactive tab
        const char *bgTabSel;   // active tab / accent
        const char *bgBtn;      // button
        const char *bgBtnHov;   // button hover
        const char *fg;         // primary text
        const char *fgDim;      // secondary / disabled text
        const char *fgHeader;   // header text
        const char *border;     // default border
        const char *borderFoc;  // focused border
        const char *accent;     // accent (selection, checkbox, links)
        const char *scrollBg;   // scroll track
        const char *scrollHnd;  // scroll handle
        const char *scrollHndH; // scroll handle hover
        const char *splitter;   // splitter handle
        const char *tooltip;    // tooltip bg
    };

    static const Tokens dark = {
        "#1b1d22",  // bg0
        "#20242c",  // bg1
        "#2b313b",  // bg2
        "#15181d",  // bgInput
        "#12161c",  // bgTable
        "#181d25",  // bgTableAlt
        "#252a33",  // bgHeader
        "#232831",  // bgDock
        "#20242c",  // bgMenu
        "#232831",  // bgTab
        "#1f6feb",  // bgTabSel
        "#272c34",  // bgBtn
        "#2f3640",  // bgBtnHov
        "#e7eaee",  // fg
        "#7f8793",  // fgDim
        "#f4f7fb",  // fgHeader
        "#313845",  // border
        "#2d7ff9",  // borderFoc
        "#2d7ff9",  // accent
        "#12161c",  // scrollBg
        "#39414d",  // scrollHnd
        "#4a5563",  // scrollHndH
        "#242931",  // splitter
        "#10141b",  // tooltip
    };

    static const Tokens light = {
        "#f6f8fa",  // bg0
        "#ffffff",  // bg1
        "#eaeef2",  // bg2
        "#ffffff",  // bgInput
        "#ffffff",  // bgTable
        "#f6f8fa",  // bgTableAlt
        "#f0f3f6",  // bgHeader
        "#eaeef2",  // bgDock
        "#ffffff",  // bgMenu
        "#eaeef2",  // bgTab
        "#0969da",  // bgTabSel
        "#f0f3f6",  // bgBtn
        "#e5eaef",  // bgBtnHov
        "#1e2128",  // fg
        "#656d76",  // fgDim
        "#1e2128",  // fgHeader
        "#d0d7de",  // border
        "#0969da",  // borderFoc
        "#0969da",  // accent
        "#e8ecf0",  // scrollBg
        "#b0bac4",  // scrollHnd
        "#8899a6",  // scrollHndH
        "#d0d7de",  // splitter
        "#ffffff",  // tooltip
    };

    const Tokens &t = (theme == UiTheme::Dark) ? dark : light;

    const QString monoFontSpec = QString("font-family: \"%1\"; font-size: %2pt;")
        .arg(monoFamily).arg(monoSizePt);
    const QString uiFontSpec = "font-family: \"Segoe UI\"; font-size: 9pt;";

    return QString(R"(
QWidget {
    background-color: %1;
    color: %2;
    selection-background-color: %3;
    selection-color: #ffffff;
    %4
}
QWidget:disabled { color: %5; }

QMainWindow, QDialog, QDockWidget, QStatusBar, QToolBar {
    background-color: %1;
    color: %2;
}

QToolTip {
    background-color: %6;
    color: %2;
    border: 1px solid %7;
    padding: 4px 6px;
}

QMenuBar {
    background-color: %1;
    color: %2;
    border-bottom: 1px solid %7;
}
QMenuBar::item { background: transparent; padding: 5px 10px; }
QMenuBar::item:selected { background-color: %8; color: %2; }

QMenu {
    background-color: %9;
    color: %2;
    border: 1px solid %7;
    padding: 6px;
}
QMenu::item { padding: 6px 22px; border-radius: 4px; }
QMenu::item:selected { background-color: %3; color: #ffffff; }
QMenu::separator { height: 1px; background-color: %7; margin: 6px 8px; }

QToolBar {
    border: none;
    border-bottom: 1px solid %7;
    padding: 4px 6px;
    spacing: 4px;
}

QToolButton, QPushButton {
    background-color: %10;
    color: %2;
    border: 1px solid %7;
    border-radius: 4px;
    padding: 4px 10px;
    min-height: 20px;
}
QToolButton:hover, QPushButton:hover {
    background-color: %11;
    border-color: %5;
}
QToolButton:pressed, QPushButton:pressed,
QToolButton:checked, QPushButton:checked,
QPushButton:default {
    background-color: %12;
    color: #ffffff;
    border-color: %12;
}
QToolButton:disabled, QPushButton:disabled {
    background-color: %1;
    color: %5;
    border-color: %7;
}

QLineEdit, QTextEdit, QPlainTextEdit, QAbstractSpinBox, QComboBox {
    background-color: %13;
    color: %2;
    border: 1px solid %7;
    border-radius: 4px;
    padding: 3px 6px;
    selection-background-color: %3;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus,
QAbstractSpinBox:focus, QComboBox:focus { border-color: %14; }

QComboBox::drop-down { width: 20px; border: none; }
QComboBox QAbstractItemView {
    background-color: %13;
    color: %2;
    border: 1px solid %7;
    selection-background-color: %3;
}

QTabWidget::pane {
    background-color: %1;
    border: 1px solid %7;
    top: -1px;
}
QTabBar::tab {
    background-color: %15;
    color: %5;
    border: 1px solid %7;
    padding: 5px 12px;
    margin-right: 2px;
}
QTabBar::tab:selected { background-color: %12; color: #ffffff; border-color: %12; }
QTabBar::tab:hover:!selected { background-color: %8; color: %2; }

QGroupBox {
    border: 1px solid %7;
    border-radius: 6px;
    margin-top: 14px;
    padding-top: 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 4px;
    color: %2;
}

QDockWidget {
    border: 1px solid %7;
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
}
QDockWidget::title {
    background-color: %16;
    color: %2;
    text-align: left;
    padding: 4px 8px;
    border-bottom: 1px solid %7;
    font-weight: 500;
}
QDockWidget::close-button, QDockWidget::float-button {
    background: transparent;
    border: none;
    padding: 2px;
}
QDockWidget::close-button:hover, QDockWidget::float-button:hover {
    background-color: %8;
}

QStatusBar {
    background-color: %1;
    color: %5;
    border-top: 1px solid %7;
}
QStatusBar::item { border: none; }

QAbstractScrollArea, QListView, QListWidget {
    background-color: %17;
    color: %2;
    border: 1px solid %7;
}

QTableView, QTableWidget, QTreeView, QTreeWidget,
QListView, QListWidget {
    background-color: %17;
    alternate-background-color: %18;
    color: %2;
    border: 1px solid %7;
    gridline-color: %7;
    selection-background-color: %3;
    selection-color: #ffffff;
    outline: 0;
    %19
}
QTableView::item:selected, QTableWidget::item:selected,
QTreeView::item:selected, QTreeWidget::item:selected,
QListView::item:selected, QListWidget::item:selected { color: #ffffff; }

QHeaderView::section {
    background-color: %20;
    color: %21;
    border: none;
    border-right: 1px solid %7;
    border-bottom: 1px solid %7;
    padding: 3px 8px;
    font-weight: 500;
}
QTableCornerButton::section {
    background-color: %20;
    border: none;
    border-right: 1px solid %7;
    border-bottom: 1px solid %7;
}

QScrollBar:vertical {
    background-color: %22;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background-color: %23;
    border-radius: 5px;
    min-height: 20px;
    margin: 1px;
}
QScrollBar::handle:vertical:hover { background-color: %24; }

QScrollBar:horizontal {
    background-color: %22;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background-color: %23;
    border-radius: 5px;
    min-width: 20px;
    margin: 1px;
}
QScrollBar::handle:horizontal:hover { background-color: %24; }
QScrollBar::add-line, QScrollBar::sub-line,
QScrollBar::add-page, QScrollBar::sub-page { background: none; border: none; }

QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator, QRadioButton::indicator { width: 14px; height: 14px; }
QCheckBox::indicator:unchecked {
    background-color: %13;
    border: 1px solid %5;
    border-radius: 3px;
}
QCheckBox::indicator:checked {
    background-color: %12;
    border: 1px solid %12;
    border-radius: 3px;
}
QRadioButton::indicator:unchecked {
    background-color: %13;
    border: 1px solid %5;
    border-radius: 7px;
}
QRadioButton::indicator:checked {
    background-color: %12;
    border: 1px solid %12;
    border-radius: 7px;
}

QSplitter::handle { background-color: %25; }
QSplitter::handle:hover { background-color: %7; }

QLabel#aboutEyebrow { color: %5; font: 600 9pt "Segoe UI"; }
QLabel#aboutTitle   { color: %2; font: 600 20pt "Segoe UI"; }
QLabel#aboutTagline { color: %5; font: 10pt "Segoe UI"; }
)")
    // %1..%25 — in order of first use in the QSS above
    .arg(t.bg0)        // 1  widget bg
    .arg(t.fg)         // 2  primary text
    .arg(t.accent)     // 3  selection bg
    .arg(uiFontSpec)   // 4  ui font
    .arg(t.fgDim)      // 5  disabled / secondary text
    .arg(t.tooltip)    // 6  tooltip bg
    .arg(t.border)     // 7  default border
    .arg(t.bg2)        // 8  hover surface
    .arg(t.bgMenu)     // 9  menu bg
    .arg(t.bgBtn)      // 10 button bg
    .arg(t.bgBtnHov)   // 11 button hover bg
    .arg(t.bgTabSel)   // 12 accent / active tab / pressed btn
    .arg(t.bgInput)    // 13 input bg
    .arg(t.borderFoc)  // 14 focus border
    .arg(t.bgTab)      // 15 inactive tab bg
    .arg(t.bgDock)     // 16 dock title bg
    .arg(t.bgTable)    // 17 table bg
    .arg(t.bgTableAlt) // 18 alternating row
    .arg(monoFontSpec) // 19 table font
    .arg(t.bgHeader)   // 20 header bg
    .arg(t.fgHeader)   // 21 header text
    .arg(t.scrollBg)   // 22 scroll track
    .arg(t.scrollHnd)  // 23 scroll handle
    .arg(t.scrollHndH) // 24 scroll handle hover
    .arg(t.splitter);  // 25 splitter
}
