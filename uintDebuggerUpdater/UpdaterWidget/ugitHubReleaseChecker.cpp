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
#include "ugitHubReleaseChecker.h"

#include "uintDebuggerVersion.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

namespace
{
QVersionNumber ToVersionNumber(const QString &versionString)
{
    return QVersionNumber::fromString(UGitHubReleaseChecker::normalizeVersionString(versionString));
}
}

UGitHubReleaseChecker::UGitHubReleaseChecker(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this)),
      m_reply(0)
{
    qRegisterMetaType<UGitHubRelease>("UGitHubRelease");
}

UGitHubReleaseChecker::~UGitHubReleaseChecker()
{
    if(m_reply != 0)
    {
        m_reply->deleteLater();
        m_reply = 0;
    }
}

void UGitHubReleaseChecker::checkLatestRelease()
{
    if(m_reply != 0)
    {
        m_reply->disconnect(this);
        m_reply->deleteLater();
        m_reply = 0;
    }

    QNetworkRequest request(latestReleaseApiUrl());
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral(UINTDEBUGGER_UPDATE_USER_AGENT));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_networkManager->get(request);
    connect(m_reply, SIGNAL(finished()), this, SLOT(slot_replyFinished()));
}

bool UGitHubReleaseChecker::isNewerThanCurrent(const QString &releaseVersionString)
{
    const QVersionNumber currentVersion = ToVersionNumber(QStringLiteral(UINTDEBUGGER_VERSION_STRING));
    const QVersionNumber releaseVersion = ToVersionNumber(releaseVersionString);

    if(releaseVersion.isNull())
        return false;

    return QVersionNumber::compare(releaseVersion, currentVersion) > 0;
}

QString UGitHubReleaseChecker::normalizeVersionString(const QString &versionString)
{
    QString normalized = versionString.trimmed();

    while(normalized.startsWith('v', Qt::CaseInsensitive))
        normalized.remove(0, 1);

    return normalized;
}

QUrl UGitHubReleaseChecker::latestReleaseApiUrl()
{
    return QUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
        .arg(QStringLiteral(UINTDEBUGGER_GITHUB_OWNER))
        .arg(QStringLiteral(UINTDEBUGGER_GITHUB_REPOSITORY)));
}

void UGitHubReleaseChecker::slot_replyFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = 0;

    if(reply == 0)
        return;

    const QByteArray payload = reply->readAll();

    if(reply->error() != QNetworkReply::NoError)
    {
        emit signal_error(reply->errorString());
        reply->deleteLater();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if(parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        emit signal_error(QStringLiteral("Failed to parse GitHub release metadata: %1").arg(parseError.errorString()));
        reply->deleteLater();
        return;
    }

    const QJsonObject root = document.object();
    UGitHubRelease release;
    release.tagName = root.value(QStringLiteral("tag_name")).toString();
    release.versionString = normalizeVersionString(release.tagName);
    release.htmlUrl = root.value(QStringLiteral("html_url")).toString();
    release.body = root.value(QStringLiteral("body")).toString();

    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for(QJsonArray::const_iterator it = assets.constBegin(); it != assets.constEnd(); ++it)
    {
        if(!it->isObject())
            continue;

        const QJsonObject assetObject = it->toObject();
        UGitHubReleaseAsset asset;
        asset.name = assetObject.value(QStringLiteral("name")).toString();
        asset.downloadUrl = QUrl(assetObject.value(QStringLiteral("browser_download_url")).toString());
        asset.size = assetObject.value(QStringLiteral("size")).toVariant().toLongLong();

        if(!asset.name.isEmpty() && asset.downloadUrl.isValid())
            release.assets.insert(asset.name, asset);
    }

    if(!release.isValid())
    {
        emit signal_error(QStringLiteral("GitHub release metadata did not contain a usable version"));
        reply->deleteLater();
        return;
    }

    emit signal_releaseReady(release);
    reply->deleteLater();
}
