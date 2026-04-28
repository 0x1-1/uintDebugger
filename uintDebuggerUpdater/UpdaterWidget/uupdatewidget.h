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
#ifndef UUPDATEWIDGET_H
#define UUPDATEWIDGET_H

#include <QMainWindow>
#include <QUrl>

class QStackedWidget;
class QProgressBar;
class QLabel;
class QPushButton;

class UCheckUpdatesWidget;
class UUpdatesModel;
class UUpdatesTableView;
class UFileDownloader;

namespace Ui {
class UUpdateWidget;
}

class UUpdateWidget : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit UUpdateWidget(QWidget *parent = 0);
    ~UUpdateWidget();

    static void OpenAndCheckForUpdates(QWidget *parent = 0);
    static void ScheduleStartupUpdateCheck(QWidget *parent);

private:
    void createFolders(const QString &path);
    QString applicationRootDir() const;
    QString updatesRootDir() const;
    void setHeaderState(const QString &title, const QString &subtitle, const QString &badge, const QString &state);
    void setActionsEnabled(bool installEnabled, bool checkEnabled, bool quitEnabled = true);
    
signals:
    void signal_downloadUpdatesFinished();

public slots:
    void slot_installUpdates();
    void slot_checkUpdates();
    void slot_checkUpdatesFailed(const QString &error);
    void slot_closeWidget();
    void slot_showUpdatesTable(UUpdatesModel *model);

protected slots:
    void slot_downloadFileFinished();
    void slot_error(const QString &error);

private:
    Ui::UUpdateWidget   *ui;

    QStackedWidget      *m_stackedWidget;
    UCheckUpdatesWidget *m_checkUpdatesWidget;
    UUpdatesTableView   *m_updatesTableView;
    QList<QProgressBar *>   m_progressBarList;

    QLabel              *m_titleLabel;
    QLabel              *m_subtitleLabel;
    QLabel              *m_badgeLabel;
    QLabel              *m_summaryLabel;
    QPushButton         *m_installButton;
    QPushButton         *m_checkButton;
    QPushButton         *m_quitButton;

    UFileDownloader     *m_downloader;

    unsigned int        m_currentDownloadFile;
};

#endif // UUPDATEWIDGET_H
