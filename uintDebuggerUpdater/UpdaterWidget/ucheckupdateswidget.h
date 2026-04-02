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
#ifndef UCHECKUPDATESWIDGET_H
#define UCHECKUPDATESWIDGET_H

#include <QWidget>
#include "ugitHubReleaseChecker.h"

namespace Ui {
class UCheckUpdatesWidget;
}

class UUpdatesModel;
class UFileDownloader;
class UGitHubReleaseChecker;

class UCheckUpdatesWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit UCheckUpdatesWidget(QWidget *parent = 0);
    ~UCheckUpdatesWidget();

    void checkUpdates();

signals:
    void signal_downloadFailed(const QString &);
    void signal_downloadFinished(const QString &msg = "");
    void signal_processUpdatesFinished(UUpdatesModel *model);

protected slots:
    void slot_releaseReady(const UGitHubRelease &release);
    void slot_manifestDownloaded();
    void slot_downloaderError(const QString &msg);

private:
    Ui::UCheckUpdatesWidget     *ui;
    UFileDownloader             *m_downloader;
    UGitHubReleaseChecker       *m_releaseChecker;
    UUpdatesModel               *m_updatesModel;
    UGitHubRelease              m_release;
    QString                     m_manifestPath;
};

#endif // UCHECKUPDATESWIDGET_H
