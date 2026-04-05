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
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QTimer>

#include "ucheckupdateswidget.h"
#include "uupdatesmodel.h"
#include "uupdatestableview.h"
#include "ufiledownloader.h"

UUpdateWidget::UUpdateWidget(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::UUpdateWidget)
{
    ui->setupUi(this);
	this->setAttribute(Qt::WA_DeleteOnClose,true);

    setFixedSize(size());

    ui->menubar->setHidden(true);
    ui->statusbar->setHidden(true);
    ui->toolBar->addAction(QPixmap(), "Download and install", this, SLOT(slot_downloadFileFinished()));
    ui->toolBar->addAction(QPixmap(), "Quit", this, SLOT(slot_closeWidget()));
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(QPixmap(), "Check updates", this, SLOT(slot_checkUpdates()));

    m_toolBarActions = ui->toolBar->actions();
    m_toolBarActions.at(eINSTALL_UPDATES_ACTION)->setEnabled(false);

    m_stackedWidget = new QStackedWidget(this);
    m_checkUpdatesWidget = new UCheckUpdatesWidget(m_stackedWidget);
    m_updatesTableView = new UUpdatesTableView(m_stackedWidget);

    m_stackedWidget->addWidget(new QWidget(m_stackedWidget));
    m_stackedWidget->addWidget(m_checkUpdatesWidget);
    m_stackedWidget->addWidget(m_updatesTableView);

    m_stackedWidget->setCurrentIndex(0);

    setCentralWidget(m_stackedWidget);

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
                .arg(QStringLiteral(UINTDEBUGGER_VERSION_STRING))
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
void UUpdateWidget::slot_installUpdates()
{
    m_toolBarActions.at(eCHECK_UPDATES_ACTION)->setEnabled(true);

    m_stackedWidget->setCurrentIndex(0);
    m_toolBarActions.at(eINSTALL_UPDATES_ACTION)->setEnabled(false);
    m_toolBarActions.at(eCHECK_UPDATES_ACTION)->setEnabled(true);

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
        return;
    }

    if(!QProcess::startDetached(updaterPath, paramList, applicationRootDir()))
    {
        QMessageBox::critical(this, tr("uintDebugger updater"),
            tr("Failed to launch updater.exe to apply the downloaded files."));
        return;
    }

    QCoreApplication::quit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_checkUpdates()
{
    m_currentDownloadFile = -1;

    m_toolBarActions.at(eCHECK_UPDATES_ACTION)->setEnabled(false);

    m_stackedWidget->setCurrentIndex(1);
    m_checkUpdatesWidget->checkUpdates();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_checkUpdatesFailed(const QString &error)
{
    m_stackedWidget->setCurrentIndex(0);
    m_toolBarActions.at(eINSTALL_UPDATES_ACTION)->setEnabled(false);
    m_toolBarActions.at(eCHECK_UPDATES_ACTION)->setEnabled(true);

    QMessageBox msgBox;
    msgBox.setText(error);
    msgBox.exec();
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

        QMessageBox::information(this, tr("uintDebugger updater"),
                                        tr("No updates are available."));

        m_stackedWidget->setCurrentIndex(0);
        m_toolBarActions.at(eINSTALL_UPDATES_ACTION)->setEnabled(false);
        m_toolBarActions.at(eCHECK_UPDATES_ACTION)->setEnabled(true);

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

        progressBar->setTextVisible(false);
        progressBar->setMinimum(0);
        progressBar->setMaximum(0);        

        m_progressBarList.append(progressBar);

        m_updatesTableView->setIndexWidget(model->index(i, UUpdatesModel::eSTATUS), m_progressBarList.last());
    }

    m_updatesTableView->resizeColumnsToContents();
    m_updatesTableView->setColumnHidden(UUpdatesModel::eURI, true);

    m_stackedWidget->setCurrentIndex(2);
    m_toolBarActions.at(eINSTALL_UPDATES_ACTION)->setEnabled(true);
    m_toolBarActions.at(eCHECK_UPDATES_ACTION)->setEnabled(false);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_downloadFileFinished()
{
    m_toolBarActions.at(eINSTALL_UPDATES_ACTION)->setEnabled(false);

    m_currentDownloadFile++;

    QAbstractTableModel *model = (UUpdatesModel *)m_updatesTableView->model();

    if (m_currentDownloadFile == model->rowCount()) {
        Q_EMIT signal_downloadUpdatesFinished();

        return;
    }

    m_downloader->set_progressBar(m_progressBarList.at(m_currentDownloadFile));

    QModelIndex index = model->index(m_currentDownloadFile, UUpdatesModel::ePACKAGE);

    const QString relativePath = QDir::fromNativeSeparators(model->data(index, Qt::DisplayRole).toString());
    createFolders(relativePath);

    const QString targetPath = QDir(updatesRootDir()).filePath(relativePath);
    index = model->index(m_currentDownloadFile, UUpdatesModel::eURI);
    m_downloader->slot_downloadFile(model->data(index, Qt::DisplayRole).toUrl(), targetPath);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UUpdateWidget::slot_error(const QString &error)
{
    qDebug() << __FUNCTION__ << ':' << error;

    // Abort the entire update on any download error.
    // Silently skipping a failed file risks a partial/broken installation.
    m_toolBarActions.at(eINSTALL_UPDATES_ACTION)->setEnabled(false);
    m_toolBarActions.at(eCHECK_UPDATES_ACTION)->setEnabled(true);
    m_stackedWidget->setCurrentIndex(0);
    m_currentDownloadFile = -1;

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

