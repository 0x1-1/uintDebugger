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
#include "ucheckupdateswidget.h"
#include "ui_ucheckupdateswidget.h"

#include "ugitHubReleaseChecker.h"
#include "uintDebuggerVersion.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include "ufiledownloader.h"
#include "uupdatesmodel.h"

namespace
{
struct UManifestEntry
{
    QString relativePath;
    QString assetName;
    QByteArray sha256;
    qint64 size = 0;
};

QString AppDirPath()
{
    return QCoreApplication::applicationDirPath();
}

QString ManifestDownloadPath()
{
    return QDir(AppDirPath()).filePath(QStringLiteral("uintDebugger-update-manifest.download"));
}

QString HumanReadableSize(qint64 size)
{
    double displaySize = static_cast<double>(size);
    QStringList units;
    units << QStringLiteral("B") << QStringLiteral("KB") << QStringLiteral("MB") << QStringLiteral("GB");
    int index = 0;

    while(displaySize >= 1024.0 && index < (units.size() - 1))
    {
        displaySize /= 1024.0;
        ++index;
    }

    if(index == 0)
        return QStringLiteral("%1 %2").arg(size).arg(units.at(index));

    return QStringLiteral("%1 %2").arg(QString::number(displaySize, 'f', 1)).arg(units.at(index));
}

QByteArray ComputeSha256(const QString &absoluteFilePath, bool *fileOpened = 0)
{
    QFile file(absoluteFilePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        if(fileOpened != 0)
            *fileOpened = false;
        return QByteArray();
    }

    if(fileOpened != 0)
        *fileOpened = true;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result().toHex().toLower();
}

bool ParseManifestFile(const QString &manifestPath, QList<UManifestEntry> *entries, QString *errorMessage)
{
    QFile file(manifestPath);
    if(!file.open(QIODevice::ReadOnly))
    {
        if(errorMessage != 0)
            *errorMessage = QStringLiteral("Unable to open downloaded update manifest");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if(parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if(errorMessage != 0)
            *errorMessage = QStringLiteral("Invalid update manifest: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    if(files.isEmpty())
    {
        if(errorMessage != 0)
            *errorMessage = QStringLiteral("Update manifest does not contain any files");
        return false;
    }

    entries->clear();
    for(QJsonArray::const_iterator it = files.constBegin(); it != files.constEnd(); ++it)
    {
        if(!it->isObject())
            continue;

        const QJsonObject fileObject = it->toObject();
        UManifestEntry entry;
        entry.relativePath = QDir::fromNativeSeparators(fileObject.value(QStringLiteral("path")).toString());
        entry.assetName = fileObject.value(QStringLiteral("asset")).toString();
        entry.sha256 = fileObject.value(QStringLiteral("sha256")).toString().trimmed().toLatin1().toLower();
        entry.size = fileObject.value(QStringLiteral("size")).toVariant().toLongLong();

        if(entry.relativePath.isEmpty() || entry.assetName.isEmpty())
            continue;

        entries->append(entry);
    }

    if(entries->isEmpty())
    {
        if(errorMessage != 0)
            *errorMessage = QStringLiteral("Update manifest does not contain valid file entries");
        return false;
    }

    return true;
}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UCheckUpdatesWidget::UCheckUpdatesWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UCheckUpdatesWidget),
    m_downloader(new UFileDownloader),
    m_releaseChecker(new UGitHubReleaseChecker(this)),
    m_updatesModel(0),
    m_manifestPath(ManifestDownloadPath())
{
    ui->setupUi(this);

    connect(m_downloader, SIGNAL(signal_error(QString)), this, SLOT(slot_downloaderError(QString)));
    connect(m_releaseChecker, SIGNAL(signal_releaseReady(UGitHubRelease)), this, SLOT(slot_releaseReady(UGitHubRelease)));
    connect(m_releaseChecker, SIGNAL(signal_error(QString)), this, SIGNAL(signal_downloadFailed(QString)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UCheckUpdatesWidget::~UCheckUpdatesWidget()
{
    delete ui;

    if(m_downloader != 0)
    {
        delete m_downloader;
        m_downloader = 0;
    }

    if(m_updatesModel != 0)
    {
        delete m_updatesModel;
        m_updatesModel = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UCheckUpdatesWidget::checkUpdates()
{
    ui->status->setText(tr("Checking latest GitHub release..."));
    ui->progressBar->setMaximum(0);
    ui->progressBar->setValue(-1);

    if(m_updatesModel != 0)
    {
        delete m_updatesModel;
        m_updatesModel = 0;
    }

    QFile::remove(m_manifestPath);
    disconnect(m_downloader, SIGNAL(signal_downloadFileFinished()), this, SLOT(slot_manifestDownloaded()));
    connect(m_downloader, SIGNAL(signal_downloadFileFinished()), this, SLOT(slot_manifestDownloaded()));

    m_releaseChecker->checkLatestRelease();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UCheckUpdatesWidget::slot_releaseReady(const UGitHubRelease &release)
{
    m_release = release;
    const UGitHubReleaseAsset manifestAsset = release.assets.value(QStringLiteral(UINTDEBUGGER_UPDATE_MANIFEST_ASSET_NAME));
    if(manifestAsset.name.isEmpty() || !manifestAsset.downloadUrl.isValid())
    {
        emit signal_downloadFailed(QStringLiteral("Latest release does not contain %1").arg(QStringLiteral(UINTDEBUGGER_UPDATE_MANIFEST_ASSET_NAME)));
        return;
    }

    ui->status->setText(tr("Downloading update manifest for %1...").arg(release.versionString));
    m_downloader->slot_downloadFile(manifestAsset.downloadUrl, m_manifestPath);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UCheckUpdatesWidget::slot_manifestDownloaded()
{
    disconnect(m_downloader, SIGNAL(signal_downloadFileFinished()), this, SLOT(slot_manifestDownloaded()));

    QList<UManifestEntry> manifestEntries;
    QString errorMessage;
    if(!ParseManifestFile(m_manifestPath, &manifestEntries, &errorMessage))
    {
        emit signal_downloadFailed(errorMessage);
        return;
    }

    if(m_updatesModel != 0)
    {
        delete m_updatesModel;
        m_updatesModel = 0;
    }

    m_updatesModel = new UUpdatesModel;

    int currentRow = 0;
    for(QList<UManifestEntry>::const_iterator it = manifestEntries.constBegin(); it != manifestEntries.constEnd(); ++it)
    {
        const QString absoluteFilePath = QDir(AppDirPath()).filePath(it->relativePath);
        bool fileOpened = false;
        const QByteArray currentHash = ComputeSha256(absoluteFilePath, &fileOpened);
        const bool needsUpdate = !fileOpened || currentHash != it->sha256;
        if(!needsUpdate)
            continue;

        const UGitHubReleaseAsset asset = m_release.assets.value(it->assetName);
        if(asset.name.isEmpty() || !asset.downloadUrl.isValid())
        {
            emit signal_downloadFailed(QStringLiteral("Release asset '%1' referenced in the manifest is missing").arg(it->assetName));
            return;
        }

        currentRow = m_updatesModel->rowCount();
        m_updatesModel->insertRows(currentRow, 1);
        m_updatesModel->setData(m_updatesModel->index(currentRow, UUpdatesModel::eSIZE), HumanReadableSize(it->size), Qt::DisplayRole);
        m_updatesModel->setData(m_updatesModel->index(currentRow, UUpdatesModel::ePACKAGE), it->relativePath, Qt::DisplayRole);
        m_updatesModel->setData(m_updatesModel->index(currentRow, UUpdatesModel::eURI), asset.downloadUrl.toString(), Qt::DisplayRole);
        m_updatesModel->setData(m_updatesModel->index(currentRow, UUpdatesModel::eSHA256), QString::fromLatin1(it->sha256), Qt::DisplayRole);
    }

    ui->status->setText(tr("Found %1 file(s) to update for %2").arg(m_updatesModel->rowCount()).arg(m_release.versionString));
    emit signal_processUpdatesFinished(m_updatesModel);
    emit signal_downloadFinished();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UCheckUpdatesWidget::slot_downloaderError(const QString &msg)
{
    emit signal_downloadFailed(msg);
}
