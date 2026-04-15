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
#include "qtDLGMemorySearch.h"
#include "clsHelperClass.h"

#include <Psapi.h>

#include <QMessageBox>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QShortcut>

// ============================================================================
// clsMemSearchWorker
// ============================================================================

void clsMemSearchWorker::setup(HANDLE hProc, const MemSearchPattern &pat,
                                DWORD64 startAddr, DWORD64 endAddr)
{
    m_hProc  = hProc;
    m_pattern = pat;
    m_start  = startAddr;
    m_end    = endAddr;
    m_stop.store(false);
}

void clsMemSearchWorker::requestStop()
{
    m_stop.store(true);
}

void clsMemSearchWorker::run()
{
    const int patLen = m_pattern.size();
    if (patLen == 0) return;

    // Pre-allocate a 1 MB read buffer, reused across all regions
    const SIZE_T CHUNK = 0x100000;
    QByteArray buf;
    buf.resize(static_cast<int>(CHUNK));

    DWORD64 totalBytes   = (m_end != ~0ULL && m_end > m_start) ? (m_end - m_start) : 0;
    DWORD64 scannedBytes = 0;
    DWORD64 addr         = m_start;

    while (!m_stop.load())
    {
        if (addr >= m_end) break;

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(m_hProc, reinterpret_cast<LPCVOID>(addr),
                           &mbi, sizeof(mbi)) != sizeof(mbi))
            break;

        DWORD64 regionBase = reinterpret_cast<DWORD64>(mbi.BaseAddress);
        DWORD64 regionEnd  = regionBase + mbi.RegionSize;

        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                            PAGE_EXECUTE_WRITECOPY)) &&
            !(mbi.Protect & PAGE_GUARD) &&
            !(mbi.Protect & PAGE_NOACCESS))
        {
            DWORD64 searchStart = qMax(regionBase, m_start);
            DWORD64 searchEnd   = qMin(regionEnd,  m_end);

            if (searchEnd > searchStart)
                searchRegion(searchStart,
                             static_cast<SIZE_T>(searchEnd - searchStart),
                             buf);
        }

        scannedBytes += mbi.RegionSize;
        if (totalBytes > 0)
            emit progress(static_cast<int>(
                qMin<DWORD64>(99, scannedBytes * 100 / totalBytes)));

        if (regionEnd <= addr) break; // DWORD64 overflow guard
        addr = regionEnd;
    }

    emit progress(100);
}

void clsMemSearchWorker::searchRegion(DWORD64 baseAddr, SIZE_T regionSize,
                                       QByteArray &buf)
{
    const SIZE_T CHUNK  = static_cast<SIZE_T>(buf.size());
    const int    patLen = m_pattern.size();
    SIZE_T offset = 0;

    while (offset < regionSize && !m_stop.load())
    {
        SIZE_T readSize  = qMin(CHUNK, regionSize - offset);
        SIZE_T bytesRead = 0;

        BOOL ok = ReadProcessMemory(m_hProc,
                                    reinterpret_cast<LPCVOID>(baseAddr + offset),
                                    buf.data(),
                                    readSize,
                                    &bytesRead);

        if (ok && bytesRead >= static_cast<SIZE_T>(patLen))
        {
            SIZE_T searchEnd = bytesRead - static_cast<SIZE_T>(patLen) + 1;
            const auto *raw = reinterpret_cast<const quint8 *>(buf.constData());

            for (SIZE_T i = 0; i < searchEnd && !m_stop.load(); ++i)
            {
                if (matchAt(raw + i))
                {
                    DWORD64 found = baseAddr + offset + i;
                    emit resultFound(found,
                        moduleAtAddress(found),
                        hexPreview(raw + i, qMin<size_t>(16, bytesRead - i)));
                }
            }
        }

        offset += readSize;
    }
}

bool clsMemSearchWorker::matchAt(const quint8 *data) const
{
    const int len = m_pattern.size();
    for (int i = 0; i < len; ++i)
    {
        if (!m_pattern.wild[i] && data[i] != m_pattern.bytes[i])
            return false;
    }
    return true;
}

QString clsMemSearchWorker::moduleAtAddress(DWORD64 addr) const
{
    wchar_t path[MAX_PATH];
    if (GetMappedFileNameW(m_hProc,
                           reinterpret_cast<LPVOID>(addr),
                           path, MAX_PATH) == 0)
        return QString();

    QString full = QString::fromWCharArray(path);
    int slash = full.lastIndexOf(QLatin1Char('\\'));
    return (slash >= 0) ? full.mid(slash + 1) : full;
}

QString clsMemSearchWorker::hexPreview(const quint8 *data, size_t len) const
{
    QString out;
    out.reserve(static_cast<int>(len) * 3);
    for (size_t i = 0; i < len; ++i)
    {
        if (i > 0) out += QLatin1Char(' ');
        out += QString::number(data[i], 16).rightJustified(2, QLatin1Char('0')).toUpper();
    }
    return out;
}

// ============================================================================
// qtDLGMemorySearch
// ============================================================================

qtDLGMemorySearch::qtDLGMemorySearch(QWidget *parent, int iPID)
    : QWidget(parent, Qt::Window)
    , m_iPID(iPID)
{
    setupUi(this);
    setLayout(mainLayout);
    setStyleSheet(clsHelperClass::LoadStyleSheet());

    m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                             FALSE, static_cast<DWORD>(iPID));

    tblResults->horizontalHeader()->resizeSection(0, 150); // Address
    tblResults->horizontalHeader()->resizeSection(1, 170); // Module
    tblResults->horizontalHeader()->setFixedHeight(21);

    connect(btnSearch,  &QPushButton::clicked,
            this, &qtDLGMemorySearch::OnSearchClicked);
    connect(btnClear,   &QPushButton::clicked,
            this, &qtDLGMemorySearch::OnClearClicked);
    connect(tblResults, &QTableWidget::itemDoubleClicked,
            this, &qtDLGMemorySearch::OnResultDoubleClicked);
    connect(tblResults, &QWidget::customContextMenuRequested,
            this, &qtDLGMemorySearch::OnContextMenu);
    connect(new QShortcut(Qt::Key_Escape, this), &QShortcut::activated,
            this, &QWidget::close);
}

qtDLGMemorySearch::~qtDLGMemorySearch()
{
    if (m_worker)
    {
        m_worker->requestStop();
        m_worker->wait(3000);
        delete m_worker;
    }
    if (m_hProcess)
        CloseHandle(m_hProcess);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void qtDLGMemorySearch::OnSearchClicked()
{
    if (m_worker && m_worker->isRunning())
    {
        m_worker->requestStop();
        return;
    }

    if (!m_hProcess)
    {
        QMessageBox::warning(this, QStringLiteral("Memory Search"),
                             QStringLiteral("Cannot open process for reading."));
        return;
    }

    bool    patOk  = false;
    QString errMsg;
    MemSearchPattern pat = buildPattern(cbSearchType->currentIndex(),
                                        lePattern->text().trimmed(),
                                        patOk, errMsg);
    if (!patOk)
    {
        QMessageBox::warning(this, QStringLiteral("Memory Search"),
                             QStringLiteral("Invalid pattern: ") + errMsg);
        return;
    }

    DWORD64 startAddr = 0;
    DWORD64 endAddr   = ~0ULL;
    if (!leStartAddr->text().trimmed().isEmpty())
    {
        bool ok;
        startAddr = leStartAddr->text().trimmed().toULongLong(&ok, 16);
        if (!ok) startAddr = 0;
    }
    if (!leEndAddr->text().trimmed().isEmpty())
    {
        bool ok;
        endAddr = leEndAddr->text().trimmed().toULongLong(&ok, 16);
        if (!ok) endAddr = ~0ULL;
    }

    OnClearClicked();

    if (!m_worker)
    {
        m_worker = new clsMemSearchWorker(this);
        connect(m_worker, &clsMemSearchWorker::resultFound,
                this, &qtDLGMemorySearch::OnResultFound,
                Qt::QueuedConnection);
        connect(m_worker, &clsMemSearchWorker::progress,
                this, &qtDLGMemorySearch::OnProgress,
                Qt::QueuedConnection);
        connect(m_worker, &QThread::finished,
                this, &qtDLGMemorySearch::OnWorkerFinished,
                Qt::QueuedConnection);
    }

    m_worker->setup(m_hProcess, pat, startAddr, endAddr);
    setSearching(true);
    m_worker->start();
}

void qtDLGMemorySearch::OnClearClicked()
{
    tblResults->setRowCount(0);
    m_resultCount = 0;
    lblStatus->setText(QStringLiteral("0 results"));
    progressBar->setValue(0);
}

void qtDLGMemorySearch::OnResultDoubleClicked(QTableWidgetItem *item)
{
    if (!item) return;
    QTableWidgetItem *addrItem = tblResults->item(item->row(), 0);
    if (!addrItem) return;
    bool ok;
    quint64 addr = addrItem->text().toULongLong(&ok, 16);
    if (ok)
        emit OnShowDisassembly(addr);
}

void qtDLGMemorySearch::OnContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = tblResults->itemAt(pos);
    if (!item) return;
    QTableWidgetItem *addrItem = tblResults->item(item->row(), 0);
    if (!addrItem) return;

    QMenu menu(this);
    QAction *actCopy  = menu.addAction(QStringLiteral("Copy Address"));
    QAction *actDisAs = menu.addAction(QStringLiteral("Show in Disassembler"));

    QAction *chosen = menu.exec(tblResults->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    bool ok;
    quint64 addr = addrItem->text().toULongLong(&ok, 16);
    if (!ok) return;

    if (chosen == actCopy)
        QApplication::clipboard()->setText(addrItem->text());
    else if (chosen == actDisAs)
        emit OnShowDisassembly(addr);
}

void qtDLGMemorySearch::OnResultFound(DWORD64 address, const QString &module,
                                       const QString &preview)
{
    int row = tblResults->rowCount();
    tblResults->insertRow(row);

    tblResults->setItem(row, 0,
        new QTableWidgetItem(
            QString::number(address, 16).toUpper()
                .rightJustified(16, QLatin1Char('0'))));
    tblResults->setItem(row, 1, new QTableWidgetItem(module));
    tblResults->setItem(row, 2, new QTableWidgetItem(preview));

    ++m_resultCount;
    lblStatus->setText(QString::number(m_resultCount) +
                       QStringLiteral(" result(s)"));
}

void qtDLGMemorySearch::OnProgress(int percent)
{
    progressBar->setValue(percent);
}

void qtDLGMemorySearch::OnWorkerFinished()
{
    setSearching(false);
    progressBar->setValue(100);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void qtDLGMemorySearch::setSearching(bool active)
{
    btnSearch->setText(active ? QStringLiteral("Cancel") : QStringLiteral("Search"));
    cbSearchType->setEnabled(!active);
    lePattern->setEnabled(!active);
    leStartAddr->setEnabled(!active);
    leEndAddr->setEnabled(!active);
}

MemSearchPattern qtDLGMemorySearch::buildPattern(int searchType,
                                                   const QString &input,
                                                   bool &ok, QString &errMsg)
{
    ok = false;
    MemSearchPattern pat;

    if (input.isEmpty())
    {
        errMsg = QStringLiteral("Pattern is empty");
        return pat;
    }

    if (searchType == 0) // Hex Bytes
    {
        QStringList tokens = input.split(
            QRegularExpression(QStringLiteral("\\s+")),
            Qt::SkipEmptyParts);

        for (const QString &tok : tokens)
        {
            if (tok == QLatin1String("??") || tok == QLatin1String("?"))
            {
                pat.bytes.append(0);
                pat.wild.append(true);
                continue;
            }
            if (tok.length() % 2 != 0)
            {
                errMsg = QStringLiteral("Odd-length hex token: ") + tok;
                return pat;
            }
            for (int i = 0; i < tok.length(); i += 2)
            {
                bool conv;
                quint8 b = static_cast<quint8>(tok.mid(i, 2).toUInt(&conv, 16));
                if (!conv)
                {
                    errMsg = QStringLiteral("Invalid hex byte: ") + tok.mid(i, 2);
                    return pat;
                }
                pat.bytes.append(b);
                pat.wild.append(false);
            }
        }
    }
    else if (searchType == 1) // ASCII String
    {
        const QByteArray bytes = input.toLatin1();
        for (char c : bytes)
        {
            pat.bytes.append(static_cast<quint8>(c));
            pat.wild.append(false);
        }
    }
    else // Unicode String (UTF-16LE)
    {
        for (const QChar &ch : input)
        {
            ushort u = ch.unicode();
            pat.bytes.append(static_cast<quint8>(u & 0xFF));
            pat.wild.append(false);
            pat.bytes.append(static_cast<quint8>(u >> 8));
            pat.wild.append(false);
        }
    }

    if (pat.isEmpty())
    {
        errMsg = QStringLiteral("Pattern resolved to zero bytes");
        return pat;
    }

    ok = true;
    return pat;
}
