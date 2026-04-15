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
#ifndef QTDLGMEMORYSEARCH_H
#define QTDLGMEMORYSEARCH_H

#include "ui_qtDLGMemorySearch.h"

#include <Windows.h>
#include <atomic>
#include <QThread>
#include <QVector>

// ---------------------------------------------------------------------------
// Pattern: per-byte value + wildcard flag
// ---------------------------------------------------------------------------
struct MemSearchPattern
{
    QVector<quint8> bytes;
    QVector<bool>   wild;   // wild[i] == true → any byte matches position i

    bool  isEmpty() const { return bytes.isEmpty(); }
    int   size()    const { return bytes.size(); }
};

// ---------------------------------------------------------------------------
// Background search worker
// ---------------------------------------------------------------------------
class clsMemSearchWorker : public QThread
{
    Q_OBJECT
public:
    explicit clsMemSearchWorker(QObject *parent = nullptr) : QThread(parent) {}

    void setup(HANDLE hProc, const MemSearchPattern &pat,
               DWORD64 startAddr, DWORD64 endAddr);
    void requestStop();

signals:
    void resultFound(DWORD64 address, const QString &module, const QString &preview);
    void progress(int percent);

protected:
    void run() override;

private:
    HANDLE           m_hProc  {nullptr};
    MemSearchPattern m_pattern;
    DWORD64          m_start  {0};
    DWORD64          m_end    {~0ULL};
    std::atomic<bool> m_stop  {false};

    void    searchRegion(DWORD64 baseAddr, SIZE_T regionSize, QByteArray &buf);
    bool    matchAt(const quint8 *data) const;
    QString moduleAtAddress(DWORD64 addr) const;
    QString hexPreview(const quint8 *data, size_t len) const;
};

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------
class qtDLGMemorySearch : public QWidget, public Ui_qtDLGMemorySearchClass
{
    Q_OBJECT

public:
    explicit qtDLGMemorySearch(QWidget *parent, int iPID);
    ~qtDLGMemorySearch();

signals:
    void OnShowDisassembly(quint64 address);

private slots:
    void OnSearchClicked();
    void OnClearClicked();
    void OnResultDoubleClicked(QTableWidgetItem *item);
    void OnContextMenu(const QPoint &pos);
    void OnResultFound(DWORD64 address, const QString &module, const QString &preview);
    void OnProgress(int percent);
    void OnWorkerFinished();

private:
    int                 m_iPID;
    HANDLE              m_hProcess {nullptr};
    clsMemSearchWorker *m_worker   {nullptr};
    int                 m_resultCount {0};

    void setSearching(bool active);
    static MemSearchPattern buildPattern(int searchType, const QString &input,
                                          bool &ok, QString &errMsg);
};

#endif // QTDLGMEMORYSEARCH_H
