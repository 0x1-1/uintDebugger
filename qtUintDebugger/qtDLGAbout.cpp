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
#include "qtDLGAbout.h"

#include "clsHelperClass.h"
#include "uintDebuggerVersion.h"

qtDLGAbout::qtDLGAbout(QWidget *parent, Qt::WindowFlags flags)
	: QDialog(parent, flags)
{
	ui.setupUi(this);
	setFixedSize(width(), height());
	setStyleSheet(clsHelperClass::LoadStyleSheet());
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &qtDLGAbout::accept);

	ui.versionChip->setText(QStringLiteral("v" UINTDEBUGGER_VERSION_STRING));
	ui.aboutBuildInfo->setText(
		QStringLiteral(
			"<b>Version</b><br/>" UINTDEBUGGER_VERSION_STRING
			"<br/><br/><b>Stack</b><br/>C++17, Qt 6.10.2, CMake, MSVC 2022"
			"<br/><br/><b>Targets</b><br/>x64, x86, WOW64"
			"<br/><br/><b>Maintainer</b><br/>" UINTDEBUGGER_MAINTAINER
			"<br/><br/><b>License</b><br/>GPL-3.0-or-later"));
}

qtDLGAbout::~qtDLGAbout()
{

}
