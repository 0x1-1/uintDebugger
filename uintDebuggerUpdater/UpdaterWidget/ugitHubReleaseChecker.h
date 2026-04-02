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
#ifndef UGITHUBRELEASECHECKER_H
#define UGITHUBRELEASECHECKER_H

#include <QObject>
#include <QHash>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

struct UGitHubReleaseAsset
{
    QString name;
    QUrl downloadUrl;
    qint64 size = 0;
};

struct UGitHubRelease
{
    QString tagName;
    QString versionString;
    QString htmlUrl;
    QString body;
    QHash<QString, UGitHubReleaseAsset> assets;

    bool isValid() const
    {
        return !versionString.isEmpty();
    }
};

class UGitHubReleaseChecker : public QObject
{
    Q_OBJECT

public:
    explicit UGitHubReleaseChecker(QObject *parent = 0);
    ~UGitHubReleaseChecker();

    void checkLatestRelease();

    static bool isNewerThanCurrent(const QString &releaseVersionString);
    static QString normalizeVersionString(const QString &versionString);
    static QUrl latestReleaseApiUrl();

signals:
    void signal_releaseReady(const UGitHubRelease &release);
    void signal_error(const QString &errorMessage);

private slots:
    void slot_replyFinished();

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_reply;
};

Q_DECLARE_METATYPE(UGitHubRelease)

#endif // UGITHUBRELEASECHECKER_H
