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
#include <QShortcut>

#include "qtDLGUintDebugger.h"
#include "qtDLGRegEdit.h"
#include "qtDLGAssembler.h"
#include "qtDLGDisassembler.h"
#include "qtDLGExceptionAsk.h"

#include "clsHelperClass.h"
#include "clsDisassembler.h"
#include "clsAPIImport.h"
#include "clsProjectFile.h"
#include "clsMemManager.h"
#include "clsCommandParser.h"
#include "uintDebuggerVersion.h"

#include <Psapi.h>
#include <shellapi.h>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QTimer>
#include <QMessageBox>
#include <QStatusBar>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QKeyEvent>

qtDLGUintDebugger* qtDLGUintDebugger::qtDLGMyWindow = NULL;

namespace
{
QString MainWindowTitle()
{
	return QStringLiteral("[" UINTDEBUGGER_DISPLAY_NAME " v " UINTDEBUGGER_VERSION_STRING "]");
}
}

qtDLGUintDebugger::qtDLGUintDebugger(QWidget *parent, Qt::WindowFlags flags)
	: QMainWindow(parent, flags),
	m_IsRestart(false),
	lExceptionCount(0),
	m_cmdInput(nullptr),
	m_cmdHistoryIdx(-1)
{
	qtDLGMyWindow = this;

	setupUi(this);
	setWindowTitle(MainWindowTitle());

	QAction *restartAsAdminAction = new QAction("Restart as Administrator", this);
	menuFile->insertAction(actionFile_Exit, restartAsAdminAction);
	connect(restartAsAdminAction, SIGNAL(triggered()), this, SLOT(action_FileRestartAsAdmin()));

	setAcceptDrops(true);

	QApplication::setStyle("Fusion");
	qApp->setStyleSheet(clsHelperClass::LoadStyleSheet());

	qRegisterMetaType<DWORD>("DWORD");
	qRegisterMetaType<quint64>("quint64");
	qRegisterMetaType<BPStruct>("BPStruct");
	qRegisterMetaType<HANDLE>("HANDLE");
	qRegisterMetaType<QList<callstackDisplay>>("QList<callstackDisplay>");

	clsAPIImport::LoadFunctions();

	coreBPManager	= new clsBreakpointManager;
	PEManager		= new clsPEManager;
	coreDebugger	= new clsDebugger();
	coreDisAs		= new clsDisassembler;
	dlgDetInfo		= new qtDLGDetailInfo(this,Qt::Window);
	dlgDbgStr		= new qtDLGDebugStrings(this,Qt::Window);
	dlgBPManager	= new qtDLGBreakPointManager(this,Qt::Window);
	dlgTraceWindow	= new qtDLGTrace(this,Qt::Window);
	dlgPatchManager = new qtDLGPatchManager(this,Qt::Window);
	dlgBookmark		= new qtDLGBookmark(this, Qt::Window);
	disasColor		= new disasColors;	
	settingManager	= clsAppSettings::SharedInstance();

	LoadWidgets();

	settingManager->CheckIfFirstRun();
	settingManager->LoadDebuggerSettings();
	settingManager->LoadDisassemblerColor();
	settingManager->LoadRecentDebuggedFiles(m_recentDebuggedFiles);
	
	LoadRecentFileMenu(true);

	DisAsGUI = new qtDLGDisassembler(this);
	this->setCentralWidget(DisAsGUI);

	// Callbacks from Debugger Thread to GUI
	connect(coreDebugger,SIGNAL(OnThread(DWORD,DWORD,quint64,bool,DWORD,bool)),
		dlgDetInfo,SLOT(OnThread(DWORD,DWORD,quint64,bool,DWORD,bool)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnPID(DWORD,QString,DWORD,quint64,bool)),
		dlgDetInfo,SLOT(OnPID(DWORD,QString,DWORD,quint64,bool)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnException(QString,QString,quint64,quint64,DWORD,DWORD)),
		dlgDetInfo,SLOT(OnException(QString,QString,quint64,quint64,DWORD,DWORD)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnDbgString(QString,DWORD)),
		dlgDbgStr,SLOT(OnDbgString(QString,DWORD)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnDll(QString,DWORD,quint64,bool)),
		dlgDetInfo,SLOT(OnDll(QString,DWORD,quint64,bool)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnLog(QString)),
		logView,SLOT(OnLog(QString)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(AskForException(DWORD)),this,SLOT(AskForException(DWORD)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnDebuggerBreak()),this,SLOT(OnDebuggerBreak()),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnDebuggerTerminated()),this,SLOT(OnDebuggerTerminated()),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnNewPID(QString,int)),dlgBPManager,SLOT(UpdateCompleter(QString,int)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(OnNewPID(QString,int)),dlgBookmark,SLOT(UpdateBookmarks(QString,int)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(UpdateOffsetsPatches(HANDLE,int)),dlgPatchManager,SLOT(UpdateOffsetPatch(HANDLE,int)),Qt::QueuedConnection);
	connect(coreDebugger,SIGNAL(UpdateOffsetsPatches(HANDLE,int)),dlgBookmark,SLOT(BookmarkUpdateOffsets(HANDLE,int)),Qt::QueuedConnection);

	connect(coreBPManager,SIGNAL(OnBreakpointAdded(BPStruct,int)),dlgBPManager,SLOT(OnUpdate(BPStruct,int)),Qt::QueuedConnection);
	connect(coreBPManager,SIGNAL(OnBreakpointDeleted(quint64)),dlgBPManager,SLOT(OnDelete(quint64)),Qt::QueuedConnection);

	// Callbacks from Debugger to PEManager
	connect(coreDebugger,SIGNAL(DeletePEManagerObject(QString,int)),PEManager,SLOT(CloseFile(QString,int)),Qt::QueuedConnection);

	// Actions for the MainMenu and Toolbar
	connect(actionFile_OpenNew, SIGNAL(triggered()), this, SLOT(action_FileOpenNewFile()));
	connect(actionFile_AttachTo, SIGNAL(triggered()), this, SLOT(action_FileAttachTo()));
	connect(actionFile_Detach, SIGNAL(triggered()), this, SLOT(action_FileDetach()));
	connect(actionFile_Exit, SIGNAL(triggered()), this, SLOT(action_FileTerminateGUI()));
	connect(actionFile_Load, SIGNAL(triggered()), this, SLOT(action_FileLoad()));
	connect(actionFile_Save, SIGNAL(triggered()), this, SLOT(action_FileSave()));
	connect(actionDebug_Start, SIGNAL(triggered()), this, SLOT(action_DebugStart()));
	connect(actionDebug_Stop, SIGNAL(triggered()), this, SLOT(action_DebugStop()));
	connect(actionDebug_Restart, SIGNAL(triggered()), this, SLOT(action_DebugRestart()));
	connect(actionDebug_Suspend, SIGNAL(triggered()), this, SLOT(action_DebugSuspend()));
	connect(actionDebug_Step_In, SIGNAL(triggered()), this, SLOT(action_DebugStepIn()));
	connect(actionDebug_Step_Out, SIGNAL(triggered()), this, SLOT(action_DebugStepOut()));
	connect(actionDebug_Step_Over, SIGNAL(triggered()), this, SLOT(action_DebugStepOver()));
	connect(actionOptions_About, SIGNAL(triggered()), this, SLOT(action_OptionsAbout()));
	connect(actionOptions_Options, SIGNAL(triggered()), this, SLOT(action_OptionsOptions()));
	connect(actionOptions_Update, SIGNAL(triggered()), this, SLOT(action_OptionsUpdate()));
	connect(actionWindow_Detail_Information, SIGNAL(triggered()), this, SLOT(action_WindowDetailInformation()));
	connect(actionWindow_Breakpoint_Manager, SIGNAL(triggered()), this, SLOT(action_WindowBreakpointManager()));
	connect(actionWindow_Show_Patches, SIGNAL(triggered()), this, SLOT(action_WindowPatches()));
	connect(actionWindow_Show_Memory, SIGNAL(triggered()), this, SLOT(action_WindowShowMemory()));
	connect(actionWindow_Show_Heap, SIGNAL(triggered()), this, SLOT(action_WindowShowHeap()));
	connect(actionWindow_Show_Strings, SIGNAL(triggered()), this, SLOT(action_WindowShowStrings()));
	connect(actionWindow_Show_Debug_Output, SIGNAL(triggered()), this, SLOT(action_WindowShowDebugOutput()));
	connect(actionWindow_Show_Handles, SIGNAL(triggered()), this, SLOT(action_WindowShowHandles()));
	connect(actionWindow_Show_Windows, SIGNAL(triggered()), this, SLOT(action_WindowShowWindows()));
	connect(actionWindow_Show_Functions, SIGNAL(triggered()), this, SLOT(action_WindowShowFunctions()));
	connect(actionWindow_Show_Privileges, SIGNAL(triggered()), this, SLOT(action_WindowShowPrivileges()));
	connect(actionWindow_Show_Bookmarks, SIGNAL(triggered()), this, SLOT(action_WindowShowBookmarks()));
	connect(action_Debug_Run_to_UserCode,SIGNAL(triggered()), this, SLOT(action_DebugRunToUserCode()));
	connect(actionDebug_Trace_Start, SIGNAL(triggered()), this, SLOT(action_DebugTraceStart()));
	connect(actionDebug_Trace_Stop, SIGNAL(triggered()), this, SLOT(action_DebugTraceStop()));
	connect(actionDebug_Trace_Show, SIGNAL(triggered()), this, SLOT(action_DebugTraceShow()));
	connect(actionWindow_Show_PEEditor, SIGNAL(triggered()), this, SLOT(action_WindowShowPEEditor()));

	// Callbacks to display disassembly
	connect(dlgTraceWindow,SIGNAL(OnDisplayDisassembly(quint64)),DisAsGUI,SLOT(OnDisplayDisassembly(quint64)));
	connect(cpuRegView,SIGNAL(OnDisplayDisassembly(quint64)),DisAsGUI,SLOT(OnDisplayDisassembly(quint64)));
	connect(dlgDetInfo,SIGNAL(ShowInDisassembler(quint64)),DisAsGUI,SLOT(OnDisplayDisassembly(quint64)));
	connect(coreDisAs,SIGNAL(DisAsFinished(quint64)),DisAsGUI,SLOT(OnDisplayDisassembly(quint64)),Qt::QueuedConnection);
	connect(dlgBPManager,SIGNAL(OnDisplayDisassembly(quint64)),DisAsGUI,SLOT(OnDisplayDisassembly(quint64)));
	connect(dlgBookmark,SIGNAL(ShowInDisassembler(quint64)),DisAsGUI,SLOT(OnDisplayDisassembly(quint64)));

	// Callbacks from PatchManager to GUI
	connect(dlgPatchManager,SIGNAL(OnReloadDebugger()),this,SLOT(OnDebuggerBreak()));

	// Callbacks from Disassembler GUI to GUI
	connect(DisAsGUI,SIGNAL(OnDebuggerBreak()),this,SLOT(OnDebuggerBreak()));

	// Callbacks to StateBar
	connect(dlgTraceWindow,SIGNAL(OnUpdateStatusBar(int,quint64)),this,SLOT(UpdateStateBar(int,quint64)));

	actionDebug_Trace_Stop->setDisabled(true);

	ParseCommandLineArgs();

	// Offer to restore the previous autosave after the main window is shown.
	QTimer::singleShot(0, this, SLOT(AutosaveRestore()));
}

qtDLGUintDebugger::~qtDLGUintDebugger()
{
	settingManager->SaveDebuggerSettings();
	settingManager->SaveDisassemblerColor();
	settingManager->SaveRecentDebuggedFiles(m_recentDebuggedFiles);

	delete coreBPManager;
	delete coreDebugger;
	delete coreDisAs;
	delete PEManager;
	delete settingManager;
	delete dlgDetInfo;
	delete dlgDbgStr;
	delete dlgBPManager;
	delete dlgTraceWindow;
	delete dlgPatchManager;
	delete dlgBookmark;
	delete disasColor;
	delete cpuRegView;
	delete callstackView;
	delete stackView;
	delete logView;
	delete DisAsGUI;
	delete m_pRecentFiles;
}

qtDLGUintDebugger* qtDLGUintDebugger::GetInstance()
{
	return qtDLGMyWindow;
}

void qtDLGUintDebugger::LoadWidgets()
{
	this->cpuRegView	= new qtDLGRegisters(this);
	this->callstackView = new qtDLGCallstack(this);
	this->stackView		= new qtDLGStack(this);
	this->logView		= new qtDLGLogView(this);
	this->dlgWatch		= new qtDLGWatch(this);

	this->addDockWidget(Qt::RightDockWidgetArea, this->cpuRegView);
	this->addDockWidget(Qt::BottomDockWidgetArea, this->callstackView);

	this->addDockWidget(Qt::BottomDockWidgetArea, this->stackView);
	this->addDockWidget(Qt::BottomDockWidgetArea, this->logView);
	this->addDockWidget(Qt::BottomDockWidgetArea, this->dlgWatch);
	dlgWatch->toggleViewAction()->setText("Watch");
	menuWindows->addAction(dlgWatch->toggleViewAction());

	if (!settingManager->RestoreWindowState(this))
	{
		this->splitDockWidget(this->callstackView, this->stackView, Qt::Vertical);
		this->splitDockWidget(this->stackView, this->logView, Qt::Horizontal);
	}

	// --- Command bar in status bar -------------------------------------------
	QWidget    *cmdWrapper = new QWidget(this);
	QHBoxLayout *cmdLayout = new QHBoxLayout(cmdWrapper);
	cmdLayout->setContentsMargins(4, 1, 4, 1);
	cmdLayout->setSpacing(4);

	m_cmdInput = new QLineEdit(cmdWrapper);
	m_cmdInput->setPlaceholderText("Command (? for help)");
	m_cmdInput->setMinimumWidth(320);
	m_cmdInput->setMaximumHeight(22);
	m_cmdInput->setStyleSheet("QLineEdit { font-family: monospace; font-size: 11px; }");

	// Up/down arrow key history navigation
	m_cmdInput->installEventFilter(this);

	QPushButton *cmdBtn = new QPushButton("Run", cmdWrapper);
	cmdBtn->setMaximumHeight(22);
	cmdBtn->setMaximumWidth(36);

	cmdLayout->addWidget(m_cmdInput);
	cmdLayout->addWidget(cmdBtn);
	cmdWrapper->setLayout(cmdLayout);

	stateBar->addPermanentWidget(cmdWrapper);

	connect(cmdBtn,    &QPushButton::clicked,      this, &qtDLGUintDebugger::OnCommandExecute);
	connect(m_cmdInput, &QLineEdit::returnPressed, this, &qtDLGUintDebugger::OnCommandExecute);
}

void qtDLGUintDebugger::OnCommandExecute()
{
	if(!m_cmdInput) return;
	const QString text = m_cmdInput->text().trimmed();
	if(text.isEmpty()) return;

	// History management
	if(m_cmdHistory.isEmpty() || m_cmdHistory.last() != text)
		m_cmdHistory.append(text);
	m_cmdHistoryIdx = m_cmdHistory.size(); // past-the-end = "no selection"

	m_cmdInput->clear();

	const QString result = clsCommandParser::Execute(text);
	if(!result.isEmpty() && logView)
		logView->OnLog(result);
}

bool qtDLGUintDebugger::eventFilter(QObject *obj, QEvent *ev)
{
	if(obj == m_cmdInput && ev->type() == QEvent::KeyPress)
	{
		QKeyEvent *ke = static_cast<QKeyEvent*>(ev);
		if(ke->key() == Qt::Key_Up)
		{
			if(!m_cmdHistory.isEmpty())
			{
				if(m_cmdHistoryIdx > 0)
					--m_cmdHistoryIdx;
				m_cmdInput->setText(m_cmdHistory.at(m_cmdHistoryIdx));
			}
			return true;
		}
		if(ke->key() == Qt::Key_Down)
		{
			if(!m_cmdHistory.isEmpty())
			{
				if(m_cmdHistoryIdx < m_cmdHistory.size() - 1)
				{
					++m_cmdHistoryIdx;
					m_cmdInput->setText(m_cmdHistory.at(m_cmdHistoryIdx));
				}
				else
				{
					m_cmdHistoryIdx = m_cmdHistory.size();
					m_cmdInput->clear();
				}
			}
			return true;
		}
	}
	return QMainWindow::eventFilter(obj, ev);
}

void qtDLGUintDebugger::OnDebuggerBreak()
{
	if(!coreDebugger->GetDebuggingState())
		UpdateStateBar(STATE_TERMINATE);
	else
	{
		// clear tracing stuff
		coreDebugger->SetTraceFlagForPID(coreDebugger->GetCurrentPID(), false);
		actionDebug_Trace_Stop->setEnabled(false);
		actionDebug_Trace_Start->setEnabled(true);
		qtDLGTrace::disableStatusBarTimer();

		// display callstack
		callstackView->ShowCallStack();

		// display Reg
		cpuRegView->LoadRegView();

		// display StackView
		quint64 dwEIP = NULL;
#ifdef _AMD64_
		BOOL bIsWOW64 = false;
		if(clsAPIImport::pIsWow64Process)
			clsAPIImport::pIsWow64Process(coreDebugger->GetCurrentProcessHandle(),&bIsWOW64);

		if(bIsWOW64)
		{
			dwEIP = coreDebugger->wowProcessContext.Eip;
			stackView->LoadStackView(coreDebugger->wowProcessContext.Esp,4);
		}
		else
		{
			dwEIP = coreDebugger->ProcessContext.Rip;
			stackView->LoadStackView(coreDebugger->ProcessContext.Rsp,8);
		}
#else
		dwEIP = coreDebugger->ProcessContext.Eip;
		stackView->LoadStackView(coreDebugger->ProcessContext.Esp,4);
#endif

		// display Disassembler
		DisAsGUI->OnDisplayDisassembly(dwEIP);

		// refresh watch window
		dlgWatch->Refresh();

		// update Toolbar
		UpdateStateBar(STATE_SUSPEND);
	}
}

void qtDLGUintDebugger::UpdateStateBar(int actionType, quint64 stepCount)
{
	int pidCount, tidCount, dllCount;
	{
		QReadLocker locker(&coreDebugger->m_stateLock);
		pidCount = coreDebugger->PIDs.size();
		tidCount = coreDebugger->TIDs.size();
		dllCount = coreDebugger->DLLs.size();
	}
	QString qsStateMessage = QString::asprintf("\t\tPIDs: %d  TIDs: %d  DLLs: %d  Exceptions: %d State: ",
		pidCount,
		tidCount,
		dllCount,
		lExceptionCount);

	switch(actionType)
	{
	case 1: // Running
		stateBar->setStyleSheet("QStatusBar { background-color: #123525; color: #ecfff1; font-weight: 600; }");
		qsStateMessage.append("Running");
		break;
	case 2: // Suspended
		stateBar->setStyleSheet("QStatusBar { background-color: #5d4700; color: #fff5cc; font-weight: 600; }");
		qsStateMessage.append("Suspended");
		break;
	case 3: // Terminated
		stateBar->setStyleSheet("QStatusBar { background-color: #5a1d1d; color: #ffe1e1; font-weight: 600; }");
		qsStateMessage.append("Terminated");
		break;
	case 4: // Tracing
		stateBar->setStyleSheet("QStatusBar { background-color: #163a5f; color: #eaf4ff; font-weight: 600; }");
		qsStateMessage.append(QString("Tracing - %1/s").arg(stepCount));
		break;
	}

	stateBar->showMessage(qsStateMessage);
}

void qtDLGUintDebugger::CleanGUI(bool bKeepLogBox)
{
	if(!bKeepLogBox) logView->tblLogBox->setRowCount(0);
	callstackView->tblCallstack->setRowCount(0);
	DisAsGUI->tblDisAs->setRowCount(0);
	cpuRegView->tblRegView->setRowCount(0);
	stackView->tblStack->setRowCount(0);

	dlgDetInfo->tblPIDs->setRowCount(0);
	dlgDetInfo->tblTIDs->setRowCount(0);
	dlgDetInfo->tblExceptions->setRowCount(0);
	dlgDetInfo->tblModules->setRowCount(0);

	dlgTraceWindow->tblTraceLog->setRowCount(0);

	DisAsGUI->dlgSourceViewer->listSource->clear();

	dlgDbgStr->tblDebugStrings->setRowCount(0);

	lExceptionCount = 0;
}

void qtDLGUintDebugger::ClearDebugData(bool cleanGUI)
{
	if(cleanGUI)
		CleanGUI();

	qtDLGBookmark::BookmarkClear();
	qtDLGPatchManager::ClearAllPatches();
	qtDLGTrace::disableStatusBarTimer();
	qtDLGTrace::clearTraceData();

	PEManager->CleanPEManager();
	coreBPManager->BreakpointClear();
	coreDisAs->SectionDisAs.clear();
	dlgBPManager->DeleteCompleterContent();
}

void qtDLGUintDebugger::OnDebuggerTerminated()
{
	PEManager->CleanPEManager();
	coreDisAs->SectionDisAs.clear();
	dlgBPManager->DeleteCompleterContent();
	qtDLGTrace::disableStatusBarTimer();
	qtDLGTrace::clearTraceData();
	actionDebug_Trace_Start->setEnabled(true);
	actionDebug_Trace_Stop->setEnabled(false);
	CleanGUI(true);
	this->setWindowTitle(MainWindowTitle());
	qtDLGPatchManager::ResetPatches();
	UpdateStateBar(STATE_TERMINATE);
	LoadRecentFileMenu();
	
	if(m_IsRestart)
	{
		m_IsRestart = false;
		action_DebugStart();
	}
}

void qtDLGUintDebugger::GenerateMenuCallback(QAction *qAction)
{
	m_iMenuProcessID = qAction->text().toULong(0,16);
}

void qtDLGUintDebugger::GenerateMenu(bool isAllEnabled)
{
	int activeProcessCount = 0;
	QList<DWORD> runningPIDs;

	{
		QReadLocker locker(&coreDebugger->m_stateLock);
		for(int i = 0; i < coreDebugger->PIDs.size(); i++)
		{
			if(coreDebugger->PIDs[i].bRunning)
			{
				activeProcessCount++;
				runningPIDs.append(coreDebugger->PIDs[i].dwPID);
			}
		}
	}

	if(activeProcessCount > 1)
	{
		QAction *qAction;
		QMenu menu;

		for(int i = 0; i < runningPIDs.size(); i++)
		{
			qAction = new QAction(QString::asprintf("%08X", runningPIDs[i]),this);
			menu.addAction(qAction);
		}

		if(isAllEnabled)
		{
			menu.addSeparator();
			qAction = new QAction("All",this);
			menu.addAction(qAction);
		}

		connect(&menu,SIGNAL(triggered(QAction*)),this,SLOT(GenerateMenuCallback(QAction*)));
		menu.exec(QCursor::pos());
	}
	else if(!runningPIDs.isEmpty())
	{
		m_iMenuProcessID = runningPIDs[0];
	}
}

void qtDLGUintDebugger::dragEnterEvent(QDragEnterEvent* pEvent)
{
	if(pEvent->mimeData()->hasUrls())
	{
        pEvent->acceptProposedAction();
    }
}

void qtDLGUintDebugger::dropEvent(QDropEvent* pEvent)
{
	if(pEvent->mimeData()->hasUrls())
    {
		// Use the standard QMimeData URL API. mimeData()->data("FileName") is not
		// reliable across Qt6 and Windows shell drag sources; urls() always works.
		const QList<QUrl> urls = pEvent->mimeData()->urls();
		if(urls.isEmpty())
		{
			pEvent->acceptProposedAction();
			return;
		}
		QString droppedFile = urls.first().toLocalFile();

		if(droppedFile.contains(".lnk", Qt::CaseInsensitive))
		{
			coreDebugger->SetTarget(clsHelperClass::ResolveShortcut(droppedFile));
			action_DebugStart();
		}
		else if(droppedFile.contains(".exe", Qt::CaseInsensitive))
		{
			coreDebugger->SetTarget(droppedFile);
			action_DebugStart();
		}
		else if(droppedFile.contains(".ndb", Qt::CaseInsensitive))
		{
			if(coreDebugger->GetDebuggingState())
			{
				QMessageBox::warning(this, "uintDebugger", "Please finish debugging first!", QMessageBox::Ok, QMessageBox::Ok);
				return;
			}

			bool startDebugging = false;
			clsProjectFile(false, &startDebugging, droppedFile);

			if(startDebugging)
				action_DebugStart();
		}
		else
		{
			QMessageBox::critical(this, "uintDebugger", "This seems to be an invalid file!", QMessageBox::Ok, QMessageBox::Ok);
		}

		pEvent->acceptProposedAction();
    }
}

QString qtDLGUintDebugger::AutosaveDir()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	QDir().mkpath(dir);
	return dir;
}

QString qtDLGUintDebugger::AutosavePath()
{
	// Use a target-specific filename so sessions for different targets do not
	// overwrite each other's autosave.  Fall back to the legacy name when no
	// target is set (e.g. pure project-file load without a running session).
	const QString target = coreDebugger->GetTarget();
	if(!target.isEmpty())
	{
		const QByteArray hash = QCryptographicHash::hash(
			target.toUtf8(), QCryptographicHash::Md5);
		return AutosaveDir() + "/autosave_" + QString::fromLatin1(hash.toHex()) + ".ndb";
	}
	return AutosaveDir() + "/autosave.ndb";
}

void qtDLGUintDebugger::AutosaveSave()
{
	// bSilent=true: autosave must never pop a dialog.
	clsProjectFile(true, nullptr, AutosavePath(), /*bSilent=*/true);
}

void qtDLGUintDebugger::AutosaveRestore()
{
	// 1. Look for per-target autosaves (autosave_*.ndb) and pick the newest.
	// 2. Fall back to the legacy autosave.ndb if nothing else is found.
	const QString dir = AutosaveDir();
	QString bestPath;
	QDateTime bestTime;

	QDirIterator it(dir, QStringList() << "autosave_*.ndb", QDir::Files);
	while(it.hasNext())
	{
		const QString candidate = it.next();
		const QDateTime modified = QFileInfo(candidate).lastModified();
		if(bestPath.isEmpty() || modified > bestTime)
		{
			bestPath = candidate;
			bestTime = modified;
		}
	}

	if(bestPath.isEmpty())
	{
		// Legacy fallback
		const QString legacy = dir + "/autosave.ndb";
		if(QFile::exists(legacy))
			bestPath = legacy;
	}

	if(bestPath.isEmpty())
		return;

	const auto btn = QMessageBox::question(this,
		QStringLiteral("uintDebugger"),
		QStringLiteral("An autosave from a previous session was found.\nRestore it?"),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::Yes);

	if(btn != QMessageBox::Yes)
		return;

	bool startDebugging = false;
	clsProjectFile(false, &startDebugging, bestPath, /*bSilent=*/true);
	if(startDebugging)
		action_DebugStart();
}

void qtDLGUintDebugger::closeEvent(QCloseEvent* closeEvent)
{
	clsAppSettings::SharedInstance()->SaveWindowState(this);

	if(coreDebugger->GetDebuggingState())
	{
		const auto btn = QMessageBox::question(this,
			QStringLiteral("uintDebugger"),
			QStringLiteral("Debugging is active. Save session before closing?"),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
			QMessageBox::Yes);

		if(btn == QMessageBox::Cancel)
		{
			closeEvent->ignore();
			return;
		}

		if(btn == QMessageBox::Yes)
			AutosaveSave();
	}

	closeEvent->accept();
}

void qtDLGUintDebugger::ParseCommandLineArgs()
{
	// CommandLineToArgvW correctly handles quoted paths and arguments; the naive
	// .split(" ") approach breaks whenever either path contains spaces.
	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if(!argv)
		return;

	QStringList splittedCommandLine;
	splittedCommandLine.reserve(argc);
	for(int k = 0; k < argc; ++k)
		splittedCommandLine.append(QString::fromWCharArray(argv[k]));
	LocalFree(argv);

	for(QStringList::const_iterator i = splittedCommandLine.constBegin(); i != splittedCommandLine.constEnd(); ++i)
	{
		if(i->compare("-p") == 0)
		{
			i++;
			if(i == splittedCommandLine.constEnd()) return;
			int PID = i->toULong();

			HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,false,PID);
			if(hProc == NULL) return;

			PTCHAR processFile = (PTCHAR)clsMemManager::CAlloc(MAX_PATH * sizeof(TCHAR));
			if(GetModuleFileNameEx(hProc,NULL,processFile,MAX_PATH) <= 0)
			{
				CloseHandle(hProc);
				clsMemManager::CFree(processFile);
				return;
			}
			QString procFile = QString::fromWCharArray(processFile,MAX_PATH);

			CloseHandle(hProc);
			clsMemManager::CFree(processFile);
			action_DebugAttachStart(PID,procFile);
			return;
		}
		else if(i->compare("-s") == 0)
		{
			i++;
			if(i == splittedCommandLine.constEnd()) return;

			coreDebugger->SetTarget(*i);

			for(QStringList::const_iterator commandLineSearch = splittedCommandLine.constBegin(); commandLineSearch != splittedCommandLine.constEnd(); ++commandLineSearch)
			{
				if(commandLineSearch->compare("-c") == 0)
				{
					commandLineSearch++;
					if(commandLineSearch == splittedCommandLine.constEnd()) break;

					coreDebugger->SetCommandLine(*commandLineSearch);
				}
			}

			action_DebugStart();
			return;
		}
		else if(i->compare("-f") == 0)
		{
			i++;
			if(i == splittedCommandLine.constEnd()) return;

			QString temp = *i;

			bool startDebugging = false;
			clsProjectFile(false, &startDebugging, temp.replace("\"", ""));

			if(startDebugging)
				action_DebugStart();
		}
	}
	return;
}

void qtDLGUintDebugger::AskForException(DWORD exceptionCode)
{
	qtDLGExceptionAsk *newException = new qtDLGExceptionAsk(exceptionCode, this, Qt::Window);
	connect(newException,SIGNAL(ContinueException(int)),coreDebugger,SLOT(HandleForException(int)),Qt::QueuedConnection);

	newException->exec();
}

void qtDLGUintDebugger::LoadRecentFileMenu(bool isFirstLoad)
{
	if(!isFirstLoad)
		delete m_pRecentFiles;
	
	m_pRecentFiles = new QMenu(menuFile);
	m_pRecentFiles->setTitle("Recent Files");

	for(int i = 0; i < 5; i++)
	{
		if(m_recentDebuggedFiles.value(i).length() > 0)
			m_pRecentFiles->addAction(new QAction(m_recentDebuggedFiles.value(i),this));
	}

	menuFile->addMenu(m_pRecentFiles);
	connect(m_pRecentFiles,SIGNAL(triggered(QAction*)),this,SLOT(DebugRecentFile(QAction*)));
}

void qtDLGUintDebugger::DebugRecentFile(QAction *qAction)
{
	if(!coreDebugger->GetDebuggingState())
	{
		ClearDebugData(true);

		coreDebugger->ClearTarget();
		coreDebugger->ClearCommandLine();
		coreDebugger->SetTarget(qAction->text());
		action_DebugStart();
	}
	else
		QMessageBox::warning(this,"uintDebugger","You have a process running. Please terminate this one first!",QMessageBox::Ok,QMessageBox::Ok);
}

void qtDLGUintDebugger::InsertRecentDebuggedFile(QString fileName)
{
	QStringList tempFileList;

	tempFileList.append(fileName);

	for(int i = 0; i < 4; i++)
	{
		if(!m_recentDebuggedFiles.value(i).contains(fileName, Qt::CaseInsensitive))
			tempFileList.append(m_recentDebuggedFiles.value(i));
	}

	m_recentDebuggedFiles = tempFileList;
}
