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
#include "qtDLGUintDebugger.h"

#include "clsCrashHandler.h"
#include "clsMemManager.h"
#include "uintDebuggerVersion.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <WinBase.h>

namespace
{
void QtMessageToFile(QtMsgType type, const QMessageLogContext &, const QString &message)
{
	if(type != QtWarningMsg && type != QtCriticalMsg && type != QtFatalMsg)
		return;

	QString level = QStringLiteral("WARN");
	switch(type)
	{
	case QtWarningMsg:
		level = QStringLiteral("WARN");
		break;
	case QtCriticalMsg:
		level = QStringLiteral("CRIT");
		break;
	case QtFatalMsg:
		level = QStringLiteral("FATAL");
		break;
	default:
		break;
	}

	const QString appDir = QCoreApplication::applicationDirPath();
	QFile logFile(QDir(appDir).filePath(QStringLiteral("uintDebugger_qt.log")));
	if(logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
	{
		QTextStream stream(&logFile);
		stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz "))
			   << '[' << level << "] " << message << '\n';
		stream.flush();
	}
}
}

bool EnableDebugFlag()
{
	TOKEN_PRIVILEGES tkpNewPriv;
	LUID luid;
	HANDLE hToken = NULL;

	if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES,&hToken))
		return false;

	if (!LookupPrivilegeValue(NULL,SE_DEBUG_NAME,&luid))
	{
		CloseHandle(hToken);
		return false;
	}

	tkpNewPriv.PrivilegeCount = 1;
	tkpNewPriv.Privileges[0].Luid = luid;
	tkpNewPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	AdjustTokenPrivileges(hToken,0,&tkpNewPriv,0,0,0);
	const bool bGranted = (GetLastError() == ERROR_SUCCESS);

	CloseHandle(hToken);
	return bGranted;
}

int main(int argc, char *argv[])
{
	clsMemManager clsMManage = clsMemManager();
	
	SetUnhandledExceptionFilter(clsCrashHandler::ErrorReporter);

	if(!EnableDebugFlag())
	{
		MessageBoxW(NULL,L"ERROR, Unable to enable Debug Privilege!\r\nThis could cause problems with some features",L"uintDebugger",MB_OK);
	}

	// Qt6: high-DPI is always enabled; PassThrough allows fractional factors (e.g. 150%)
	QApplication::setHighDpiScaleFactorRoundingPolicy(
		Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

	QApplication a(argc, argv);
	a.setApplicationName(QStringLiteral(UINTDEBUGGER_DISPLAY_NAME));
	a.setApplicationVersion(QStringLiteral(UINTDEBUGGER_VERSION_DISPLAY_STRING));
	qInstallMessageHandler(QtMessageToFile);
	qtDLGUintDebugger w;
	w.show();
	return a.exec();
}
