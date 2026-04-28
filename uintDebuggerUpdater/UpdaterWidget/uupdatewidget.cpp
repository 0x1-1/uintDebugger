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
#include "uupdatewidget.h"
#include "ui_uupdatewidget.h"

#include "ugitHubReleaseChecker.h"
#include "uintDebuggerVersion.h"

#include <QProcess>

#include <QStackedWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "ucheckupdateswidget.h"
#include "uqtpathsafety.h"
#include "uupdatesmodel.h"
#include "uupdatestableview.h"
#include "ufiledownloader.h"

UUpdateWidget::UUpdateWidget(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::UUpdateWidget),
    m_titleLabel(0),
    m_subtitleLabel(0),
    m_badgeLabel(0),
    m_summaryLabel(0),
    m_installButton(0),
    m_checkButton(0),
    m_quitButton(0)
{
    ui->setupUi(this);
	this->setAttribute(Qt::WA_DeleteOnClose,true);

    setWindowTitle(tr("uintDebugger Update Manager"));
    resize(760, 460);
    setMinimumSize(680, 420);

    ui->menubar->setHidden(true);
    ui->statusbar->setHidden(true);
    ui->toolBar->setHidden(true);

    QWidget *rootWidget = new QWidget(this);
    rootWidget->setObjectName(QStringLiteral("updateRoot"));
    QVBoxLayout *rootLayout = new QVBoxLayout(rootWidget);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(0);

    QFrame *panel = new QFrame(rootWidget);
    panel->setObjectName(QStringLiteral("updatePanel"));
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(18, 16, 18, 16);
    panelLayout->setSpacing(14);

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    QVBoxLayout *titleLayout = new QVBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(4);

    m_titleLabel = new QLabel(panel);
    m_titleLabel->setObjectName(QStringLiteral("updateTitle"));
    m_subtitleLabel = new QLabel(panel);
    m_subtitleLabel->setObjectName(QStringLiteral("updateSubtitle"));
    m_subtitleLabel->setWordWrap(true);

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    m_badgeLabel = new QLabel(panel);
    m_badgeLabel->setObjectName(QStringLiteral("updateBadge"));
    m_badgeLabel->setAlignment(Qt::AlignCenter);
    m_badgeLabel->setMinimumWidth(86);

    headerLayout->addLayout(titleLayout, 1);
    headerLayout->addWidget(m_badgeLabel, 0, Qt::AlignTop);

    m_stackedWidget = new QStackedWidget(panel);
    m_stackedWidget->setObjectName(QStringLiteral("updateStack"));
    m_stackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_checkUpdatesWidget = new UCheckUpdatesWidget(m_stackedWidget);
    m_updatesTableView = new UUpdatesTableView(m_stackedWidget);

    QWidget *idlePage = new QWidget(m_stackedWidget);
    idlePage->setObjectName(QStringLiteral("idlePage"));
    QVBoxLayout *idleLayout = new QVBoxLayout(idlePage);
    idleLayout->setContentsMargins(24, 24, 24, 24);
    idleLayout->setSpacing(8);
    idleLayout->addStretch(1);
    QLabel *idleTitle = new QLabel(tr("No update check is running."), idlePage);
    idleTitle->setObjectName(QStringLiteral("idleTitle"));
    idleTitle->setAlignment(Qt::AlignCenter);
    QLabel *idleSubtitle = new QLabel(tr("Use Check updates to compare this build against the latest GitHub release."), idlePage);
    idleSubtitle->setObjectName(QStringLiteral("idleSubtitle"));
    idleSubtitle->setAlignment(Qt::AlignCenter);
    idleSubtitle->setWordWrap(true);
    idleLayout->addWidget(idleTitle);
    idleLayout->addWidget(idleSubtitle);
    idleLayout->addStretch(1);

    m_stackedWidget->addWidget(idlePage);
    m_stackedWidget->addWidget(m_checkUpdatesWidget);
    m_stackedWidget->addWidget(m_updatesTableView);

    m_stackedWidget->setCurrentIndex(0);

    QHBoxLayout *footerLayout = new QHBoxLayout;
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(8);

    m_summaryLabel = new QLabel(panel);
    m_summaryLabel->setObjectName(QStringLiteral("updateSummary"));
    m_summaryLabel->setWordWrap(true);

    m_checkButton = new QPushButton(tr("Check updates"), panel);
    m_quitButton = new QPushButton(tr("Close"), panel);
    m_installButton = new QPushButton(tr("Download and install"), panel);
    m_installButton->setObjectName(QStringLiteral("primaryButton"));
    m_installButton->setMinimumWidth(162);
    m_checkButton->setMinimumWidth(116);
    m_quitButton->setMinimumWidth(82);

    footerLayout->addWidget(m_summaryLabel, 1);
    footerLayout->addWidget(m_checkButton);
    footerLayout->addWidget(m_quitButton);
    footerLayout->addWidget(m_installButton);

    panelLayout->addLayout(headerLayout);
    panelLayout->addWidget(m_stackedWidget, 1);
    panelLayout->addLayout(footerLayout);

    rootLayout->addWidget(panel, 1);
    setCentralWidget(rootWidget);

    connect(m_installButton, SIGNAL(clicked()), this, SLOT(slot_downloadFileFinished()));
    connect(m_quitButton, SIGNAL(clicked()), this, SLOT(slot_closeWidget()));
    connect(m_checkButton, SIGNAL(clicked()), this, SLOT(slot_checkUpdates()));

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#updateRoot {
            background: #080d13;
            color: #e8eef5;
        }
        QLabel {
            background: transparent;
        }
        QFrame#updatePanel {
            background: #101722;
            border: 1px solid #2b3442;
            border-radius: 10px;
        }
        QLabel#updateTitle {
            color: #f3f7fb;
            font-size: 19px;
            font-weight: 700;
        }
        QLabel#updateSubtitle, QLabel#idleSubtitle, QLabel#updateSummary {
            color: #8fa0b4;
            font-size: 12px;
        }
        QLabel#idleTitle {
            color: #d7e4f2;
            font-size: 16px;
            font-weight: 650;
        }
        QLabel#updateBadge {
            border-radius: 13px;
            padding: 5px 10px;
            background: #1b2430;
            border: 1px solid #344255;
            color: #aebdd0;
            font-weight: 700;
        }
        QLabel#updateBadge[state="busy"] {
            background: #102b46;
            border-color: #2f78b7;
            color: #9bd2ff;
        }
        QLabel#updateBadge[state="ready"] {
            background: #11351f;
            border-color: #2f8c51;
            color: #a8f0bd;
        }
        QLabel#updateBadge[state="error"] {
            background: #3a1717;
            border-color: #944242;
            color: #ffb7b7;
        }
        QStackedWidget#updateStack {
            background: #0c121a;
            border: 1px solid #273140;
            border-radius: 8px;
        }
        QTableView {
            background: #0c121a;
            alternate-background-color: #101923;
            border: none;
            color: #e6edf5;
            selection-background-color: #1f6feb;
            selection-color: #ffffff;
            gridline-color: #243042;
        }
        QHeaderView::section {
            background: #182231;
            color: #c9d7e6;
            border: none;
            border-right: 1px solid #283548;
            border-bottom: 1px solid #283548;
            padding: 7px 9px;
            font-weight: 700;
        }
        QProgressBar {
            background: #151f2d;
            border: 1px solid #2b3a4f;
            border-radius: 7px;
            color: #eaf3ff;
            min-height: 18px;
            text-align: center;
            font-size: 11px;
            font-weight: 700;
        }
        QProgressBar::chunk {
            border-radius: 6px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #2f81f7, stop:1 #35d0ba);
        }
        QPushButton {
            background: #182231;
            border: 1px solid #344255;
            border-radius: 6px;
            color: #eef6ff;
            padding: 8px 13px;
            font-weight: 650;
        }
        QPushButton:hover {
            background: #202c3d;
            border-color: #4a5d76;
        }
        QPushButton:disabled {
            color: #68768a;
            background: #121922;
            border-color: #242d3a;
        }
        QPushButton#primaryButton {
            background: #1f6feb;
            border-color: #3f8cff;
            color: #ffffff;
        }
        QPushButton#primaryButton:hover {
            background: #2f81f7;
        }
    )"));

    setHeaderState(
        tr("Update Manager"),
        tr("Current build: %1").arg(QStringLiteral(UINTDEBUGGER_VERSION_DISPLAY_STRING)),
        tr("Idle"),
        QStringLiteral("idle"));
    m_summaryLabel->setText(tr("No update check has been run yet."));
    setActionsEnabled(false, true);

    connect(m_checkUpdatesWidget,   SIGNAL(signal_processUpdatesFinished(UUpdatesModel *)),
            this,                   SLOT(slot_showUpdatesTable(UUpdatesModel *)));
    connect(m_checkUpdatesWidget,   SIGNAL(signal_downloadFailed(QString)),
            this,                   SLOT(slot_checkUpdatesFailed(QString)));

    m_downloader = new UFileDownloader;

    connect(m_downloader,   SIGNAL(signal_downloadFileFinished()),
            this,           SLOT(slot_downloadFileFinished()));
    connect(m_downloader,   SIGNAL(signal_error(QString)),
            this,           SLOT(slot_error(QString)));

    m_currentDownloadFile = -1;

    connect(this,   SIGNAL(signal_downloadUpdatesFinished()),
            this,   SLOT(slot_installUpdates()));
}

void UUpdateWidget::OpenAndCheckForUpdates(QWidget *parent)
{
    UUpdateWidget *widget = new UUpdateWidget(parent);
    widget->show();
    widget->slot_checkUpdates();
}

void UUpdateWidget::ScheduleStartupUpdateCheck(QWidget *parent)
{
    if(parent == 0)
        return;

    QTimer::singleShot(1500, parent, [parent]()
    {
        UGitHubReleaseChecker *checker = new UGitHubReleaseChecker(parent);
        QObject::connect(checker, &UGitHubReleaseChecker::signal_releaseReady, parent, [parent, checker](const UGitHubRelease &release)
        {
            if(!UGitHubReleaseChecker::isNewerThanCurrent(release.versionString))
            {
                checker->deleteLater();
                return;
            }

            QMessageBox updatePrompt(parent);
            updatePrompt.setWindowTitle(QStringLiteral(UINTDEBUGGER_DISPLAY_NAME));
            updatePrompt.setIcon(QMessageBox::Information);
            updatePrompt.setText(QStringLiteral("A new version of %1 is available.").arg(QStringLiteral(UINTDEBUGGER_DISPLAY_NAME)));
            updatePrompt.setInformativeText(QStringLiteral("Current version: %1\nAvailable version: %2\n\nOpen the updater now?")
                .arg(QStringLiteral(UINTDEBUGGER_VERSION_DISPLAY_STRING))
                .arg(release.versionString));
            updatePrompt.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            updatePrompt.setDefaultButton(QMessageBox::Yes);

            if(updatePrompt.exec() == QMessageBox::Yes)
                UUpdateWidget::OpenAndCheckForUpdates(parent);

            checker->deleteLater();
        });
        QObject::connect(checker, &UGitHubReleaseChecker::signal_error, parent, [checker](const QString &)
        {
            checker->deleteLater();
        });
        checker->checkLatestRelease();
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UUpdateWidget::~UUpdateWidget()
{
    delete ui;

    if (m_downloader != 0) {
        delete m_downloader;
        m_downloader = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::setHeaderState(const QString &title, const QString &subtitle, const QString &badge, const QString &state)
{
    if(m_titleLabel != 0)
        m_titleLabel->setText(title);
    if(m_subtitleLabel != 0)
        m_subtitleLabel->setText(subtitle);
    if(m_badgeLabel != 0)
    {
        m_badgeLabel->setText(badge);
        m_badgeLabel->setProperty("state", state);
        m_badgeLabel->style()->unpolish(m_badgeLabel);
        m_badgeLabel->style()->polish(m_badgeLabel);
        m_badgeLabel->update();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::setActionsEnabled(bool installEnabled, bool checkEnabled, bool quitEnabled)
{
    if(m_installButton != 0)
        m_installButton->setEnabled(installEnabled);
    if(m_checkButton != 0)
        m_checkButton->setEnabled(checkEnabled);
    if(m_quitButton != 0)
        m_quitButton->setEnabled(quitEnabled);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_installUpdates()
{
    m_stackedWidget->setCurrentIndex(0);
    setActionsEnabled(false, false, false);
    setHeaderState(
        tr("Installing update"),
        tr("Launching updater.exe to replace the downloaded files."),
        tr("Install"),
        QStringLiteral("busy"));
    m_summaryLabel->setText(tr("uintDebugger will close and relaunch after the updater finishes."));

    QAbstractItemModel *model = m_updatesTableView->model();
    QStringList paramList;
    QModelIndex index;

    for (int i = 0; i < model->rowCount(); i++) {
        index = model->index(i,  UUpdatesModel::ePACKAGE);
        paramList << model->data(index).toString();
    }

    const QString updaterPath = QDir(applicationRootDir()).filePath(QStringLiteral("updater.exe"));
    if(!QFileInfo::exists(updaterPath))
    {
        QMessageBox::critical(this, tr("uintDebugger updater"),
            tr("Unable to find updater.exe next to the application binaries."));
        setActionsEnabled(false, true);
        setHeaderState(
            tr("Install blocked"),
            tr("updater.exe is missing from the application directory."),
            tr("Error"),
            QStringLiteral("error"));
        return;
    }

    if(!QProcess::startDetached(updaterPath, paramList, applicationRootDir()))
    {
        QMessageBox::critical(this, tr("uintDebugger updater"),
            tr("Failed to launch updater.exe to apply the downloaded files."));
        setActionsEnabled(false, true);
        setHeaderState(
            tr("Install failed"),
            tr("Windows refused to start updater.exe."),
            tr("Error"),
            QStringLiteral("error"));
        return;
    }

    QCoreApplication::quit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_checkUpdates()
{
    m_currentDownloadFile = -1;

    m_stackedWidget->setCurrentIndex(1);
    setActionsEnabled(false, false);
    setHeaderState(
        tr("Checking for updates"),
        tr("Contacting GitHub Releases and downloading the signed update manifest."),
        tr("Checking"),
        QStringLiteral("busy"));
    m_summaryLabel->setText(tr("Network check in progress..."));
    m_checkUpdatesWidget->checkUpdates();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_checkUpdatesFailed(const QString &error)
{
    m_stackedWidget->setCurrentIndex(0);
    setActionsEnabled(false, true);
    setHeaderState(
        tr("Update check failed"),
        error,
        tr("Error"),
        QStringLiteral("error"));
    m_summaryLabel->setText(tr("No files were downloaded. You can try the check again."));

    QMessageBox::critical(this, tr("uintDebugger updater"), error);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_closeWidget()
{
    close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_showUpdatesTable(UUpdatesModel *model)
{
    if (model->rowCount() == 0) {
        qDebug() << "there is no updates";

        m_stackedWidget->setCurrentIndex(0);
        setActionsEnabled(false, true);
        setHeaderState(
            tr("Already up to date"),
            tr("Current build: %1").arg(QStringLiteral(UINTDEBUGGER_VERSION_DISPLAY_STRING)),
            tr("Current"),
            QStringLiteral("ready"));
        m_summaryLabel->setText(tr("No files need to be replaced."));

        return;
    }

    // create folder for updates
    QDir().mkpath(updatesRootDir());
    // end of creating folder for updates

    m_updatesTableView->setModel(model);
    m_progressBarList.clear();

    QProgressBar *progressBar;
    for (int i = 0; i < model->rowCount(); i++) {
        progressBar = new QProgressBar(m_updatesTableView);

        progressBar->setTextVisible(true);
        progressBar->setFormat(tr("Queued"));
        progressBar->setMinimum(0);
        progressBar->setMaximum(100);
        progressBar->setValue(0);

        m_progressBarList.append(progressBar);

        m_updatesTableView->setIndexWidget(model->index(i, UUpdatesModel::eSTATUS), m_progressBarList.last());
    }

    m_updatesTableView->setColumnHidden(UUpdatesModel::eURI, true);
    m_updatesTableView->setColumnHidden(UUpdatesModel::eSHA256, true);
    m_updatesTableView->horizontalHeader()->setSectionResizeMode(UUpdatesModel::eSTATUS, QHeaderView::Fixed);
    m_updatesTableView->horizontalHeader()->setSectionResizeMode(UUpdatesModel::eSIZE, QHeaderView::ResizeToContents);
    m_updatesTableView->horizontalHeader()->setSectionResizeMode(UUpdatesModel::ePACKAGE, QHeaderView::Stretch);
    m_updatesTableView->setColumnWidth(UUpdatesModel::eSTATUS, 190);
    m_updatesTableView->resizeRowsToContents();

    m_stackedWidget->setCurrentIndex(2);
    setActionsEnabled(true, true);
    setHeaderState(
        tr("Update ready"),
        tr("%1 file(s) will be downloaded, verified with SHA-256, then installed.").arg(model->rowCount()),
        tr("Ready"),
        QStringLiteral("ready"));
    m_summaryLabel->setText(tr("Review the file list, then start the update when ready."));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_downloadFileFinished()
{
    setActionsEnabled(false, false);

    QAbstractTableModel *model = (UUpdatesModel *)m_updatesTableView->model();

    // m_currentDownloadFile starts at UINT_MAX (set to -1). The first call is from the
    // toolbar button, so no file has been downloaded yet. Subsequent calls are from the
    // downloader signal — verify the file that was just downloaded before proceeding.
    if(m_currentDownloadFile != (unsigned int)-1)
    {
        const QModelIndex pkgIdx    = model->index(m_currentDownloadFile, UUpdatesModel::ePACKAGE);
        const QModelIndex sha256Idx = model->index(m_currentDownloadFile, UUpdatesModel::eSHA256);

        const QString relativePath   = QDir::fromNativeSeparators(model->data(pkgIdx,    Qt::DisplayRole).toString());
        const QString expectedHash   = model->data(sha256Idx, Qt::DisplayRole).toString().trimmed().toLower();
        const QString downloadedPath = QDir(updatesRootDir()).filePath(relativePath);

        if(!expectedHash.isEmpty())
        {
            QFile file(downloadedPath);
            if(!file.open(QIODevice::ReadOnly))
            {
                slot_error(tr("Could not open downloaded file for verification: %1").arg(relativePath));
                return;
            }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(&file);
            const QString actualHash = QString::fromLatin1(hash.result().toHex().toLower());

            if(actualHash != expectedHash)
            {
                QFile::remove(downloadedPath);
                slot_error(tr("SHA-256 verification failed for %1.\nExpected: %2\nActual:   %3\n\nThe corrupted file has been removed.")
                    .arg(relativePath, expectedHash, actualHash));
                return;
            }
        }

        if(m_currentDownloadFile < (unsigned int)m_progressBarList.size())
        {
            QProgressBar *completedProgress = m_progressBarList.at(m_currentDownloadFile);
            completedProgress->setMinimum(0);
            completedProgress->setMaximum(100);
            completedProgress->setValue(100);
            completedProgress->setFormat(tr("Verified"));
        }
    }

    m_currentDownloadFile++;

    if(m_currentDownloadFile == (unsigned int)model->rowCount()) {
        setHeaderState(
            tr("Ready to install"),
            tr("All files were downloaded and verified."),
            tr("Verified"),
            QStringLiteral("ready"));
        m_summaryLabel->setText(tr("Starting updater.exe..."));
        Q_EMIT signal_downloadUpdatesFinished();
        return;
    }

    QProgressBar *activeProgress = m_progressBarList.at(m_currentDownloadFile);
    activeProgress->setMinimum(0);
    activeProgress->setMaximum(0);
    activeProgress->setValue(0);
    activeProgress->setFormat(tr("Downloading"));
    m_downloader->set_progressBar(activeProgress);

    QModelIndex index = model->index(m_currentDownloadFile, UUpdatesModel::ePACKAGE);

    const QString relativePath = QDir::fromNativeSeparators(model->data(index, Qt::DisplayRole).toString());
    setHeaderState(
        tr("Downloading update"),
        tr("File %1 of %2: %3").arg(m_currentDownloadFile + 1).arg(model->rowCount()).arg(relativePath),
        tr("Downloading"),
        QStringLiteral("busy"));
    m_summaryLabel->setText(tr("Files are downloaded into the local updates folder before install."));

    // Defence-in-depth: validate the path from the model before constructing the
    // download destination. This mirrors the validation done when the manifest was
    // first parsed; it guards against any in-memory model tampering.
    const QString updRoot = updatesRootDir();
    if(!UpdaterQtSafety::IsPathSafeForRoot(relativePath, updRoot))
    {
        slot_error(tr("Blocked unsafe download path: %1").arg(relativePath));
        return;
    }

    createFolders(relativePath);

    const QString targetPath = UpdaterQtSafety::NormalizedAbsolutePath(QDir(updRoot).absoluteFilePath(QDir::cleanPath(relativePath)));
    index = model->index(m_currentDownloadFile, UUpdatesModel::eURI);
    m_downloader->slot_downloadFile(model->data(index, Qt::DisplayRole).toUrl(), targetPath);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_error(const QString &error)
{
    qDebug() << __FUNCTION__ << ':' << error;

    // Abort the entire update on any download error.
    // Silently skipping a failed file risks a partial/broken installation.
    setActionsEnabled(false, true);
    m_stackedWidget->setCurrentIndex(0);
    m_currentDownloadFile = -1;
    setHeaderState(
        tr("Update failed"),
        error,
        tr("Error"),
        QStringLiteral("error"));
    m_summaryLabel->setText(tr("The update was cancelled before install."));

    QMessageBox::critical(this, tr("uintDebugger updater"),
        tr("Download failed: %1\n\nThe update was cancelled. Please try again.").arg(error));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::createFolders(const QString &path)
{
    const QFileInfo fileInfo(QDir(updatesRootDir()).filePath(QDir::fromNativeSeparators(path)));
    QDir().mkpath(fileInfo.absolutePath());
}

QString UUpdateWidget::applicationRootDir() const
{
    return QCoreApplication::applicationDirPath();
}

QString UUpdateWidget::updatesRootDir() const
{
    return QDir(applicationRootDir()).filePath(QStringLiteral("updates"));
}
