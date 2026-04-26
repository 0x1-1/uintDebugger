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
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
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

	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return false;

	if (!LookupPrivilegeValue(NULL,SE_DEBUG_NAME,&luid))
	{
		CloseHandle(hToken);
		return false;
	}

	tkpNewPriv.PrivilegeCount = 1;
	tkpNewPriv.Privileges[0].Luid = luid;
	tkpNewPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	SetLastError(ERROR_SUCCESS);
	AdjustTokenPrivileges(hToken,0,&tkpNewPriv,0,0,0);
	const bool bGranted = (GetLastError() == ERROR_SUCCESS);

	CloseHandle(hToken);
	return bGranted;
}

int main(int argc, char *argv[])
{
	clsMemManager clsMManage = clsMemManager();
	
	SetUnhandledExceptionFilter(clsCrashHandler::ErrorReporter);

	const bool debugPrivilegeEnabled = EnableDebugFlag();

	// Qt6: high-DPI is always enabled; PassThrough allows fractional factors (e.g. 150%)
	QApplication::setHighDpiScaleFactorRoundingPolicy(
		Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

	QApplication a(argc, argv);
	a.setApplicationName(QStringLiteral(UINTDEBUGGER_DISPLAY_NAME));
	a.setApplicationVersion(QStringLiteral(UINTDEBUGGER_VERSION_DISPLAY_STRING));
	qInstallMessageHandler(QtMessageToFile);
	qtDLGUintDebugger w;
	w.show();
	QTimer::singleShot(0, &w, [&w, debugPrivilegeEnabled]() {
		if(!w.isVisible())
			w.show();
		if(w.isMinimized())
			w.showNormal();
		w.raise();
		w.activateWindow();

		if(!debugPrivilegeEnabled)
		{
			const QString message = QStringLiteral(
				"Warning: SeDebugPrivilege could not be enabled. Run as Administrator for full debugger access.");
			qWarning().noquote() << message;
			w.statusBar()->showMessage(message, 15000);
		}
	});
	return a.exec();
}
