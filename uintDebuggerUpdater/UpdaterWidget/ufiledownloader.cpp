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
#include "ufiledownloader.h"
#include "uintDebuggerVersion.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProgressBar>
#include <QDir>
#include <QDebug>
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UFileDownloader::UFileDownloader(QObject *parent) :
    QObject(parent), m_pathDir(""), m_requestedFileName(""), m_reply(0), m_pb(0)
{
    connect(this,   SIGNAL(signal_redirectTo(QUrl)),
            this,   SLOT(slot_downloadFile(QUrl)));

    connect(this,   SIGNAL(signal_fileSizeredirectTo(QUrl)),
            this,   SLOT(slot_getFileSize(QUrl)));

    connect(&m_manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
                        SLOT(slot_sslErrors(QNetworkReply*,QList<QSslError>)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UFileDownloader::~UFileDownloader()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::set_progressBar(QProgressBar *pb)
{
    m_pb = pb;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::setDir(const QString &pathDir)
{
    m_pathDir = pathDir;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QString UFileDownloader::saveFileName(const QUrl &url)
{
    QString path = url.path();
    QString basename = QFileInfo(path).fileName();

    if (m_pathDir.isEmpty() == false) {
        basename = QCoreApplication::applicationDirPath() + '/' + m_pathDir + '/' + basename;
    }

    if (basename.isEmpty())
        basename = "download";

    if (QFile::exists(basename)) {
        QFile::remove(basename);
//        // already exists, don't overwrite
//        int i = 0;
//        basename += '.';
//        while (QFile::exists(basename + QString::number(i)))
//            ++i;

//        basename += QString::number(i);
    }

    return basename;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::slot_getFileSize(QUrl url)
{
    m_url = url;

    if (m_url.isEmpty() == true) {
        Q_EMIT signal_error("Error URL");
        return;
    }

    disconnect(&m_manager, SIGNAL(finished(QNetworkReply*)),
               this,       SLOT(slot_getFileSize(QNetworkReply*)));

    connect(&m_manager, SIGNAL(finished(QNetworkReply*)),
            this,       SLOT(slot_getFileSize(QNetworkReply*)));

    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral(UINTDEBUGGER_UPDATE_USER_AGENT));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_manager.head(request);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::slot_downloadFile(QUrl url, const QString &neededFileName)
{
    m_url = url;

    if (m_url.isEmpty() == true) {
        Q_EMIT signal_error("Error URL");
        return;
    }

    m_file.close();

    if (neededFileName.isEmpty() == false) {
        m_requestedFileName = neededFileName;
    }

    QString fileName = m_requestedFileName;
    if (fileName.isEmpty()) {
        fileName = saveFileName(m_url);
        m_requestedFileName = fileName;
    }

    m_file.setFileName(fileName);
    if (!m_file.open(QIODevice::WriteOnly)) {
        qDebug() << __FUNCTION__ << "error while opening file";

        Q_EMIT signal_error(QString("Problem opening save file %1 for download %2: %3")
                            .arg(fileName).arg(url.toEncoded().constData()).arg(m_file.errorString()));

        return;
    }

    disconnect(&m_manager, SIGNAL(finished(QNetworkReply*)),
               this,       SLOT(slot_getFileSize(QNetworkReply*)));

    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral(UINTDEBUGGER_UPDATE_USER_AGENT));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_manager.get(request);

    connect(m_reply,    SIGNAL(finished()),
                        SLOT(slot_downloadFileFinished()));
    connect(m_reply,    SIGNAL(downloadProgress(qint64,qint64)),
                        SLOT(slot_downloadProgress(qint64,qint64)));
    connect(m_reply,    SIGNAL(readyRead()),
                        SLOT(slot_readFile()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::slot_getFileSize(QNetworkReply *pReply)
{
    QVariant redirectionTarget = pReply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    if (pReply->error()) {
        qDebug() << __FUNCTION__ << " error while getting size of the file";
        Q_EMIT signal_error("Error while getting size of the file");
    } else if (!redirectionTarget.isNull()) {
        qDebug() << __FUNCTION__ << " redirected link: " << redirectionTarget.toUrl();
        Q_EMIT signal_fileSizeredirectTo(redirectionTarget.toUrl());
    } else {
        qDebug() << __FUNCTION__ << " got file size: " << pReply->header(QNetworkRequest::ContentLengthHeader).toUInt();
        Q_EMIT signal_gotFileSize(pReply->header(QNetworkRequest::ContentLengthHeader).toUInt());
    }

    pReply->deleteLater();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::slot_downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_pb != 0) {
        m_pb->setMinimum(0);
        m_pb->setMaximum(bytesTotal);
        m_pb->setValue(bytesReceived);
    }

    qDebug() << __FUNCTION__ << bytesReceived << ' ' << bytesTotal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::slot_downloadFileFinished()
{
    if (m_reply == 0) {
        return;
    }

    m_file.close();

    const QVariant redirectionTarget = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    QNetworkReply *finishedReply = m_reply;
    m_reply = 0;

    if (finishedReply->error()) {
        m_pb = 0;
        qDebug() << __FUNCTION__ << " Error while downloading file";
        m_requestedFileName.clear();
        Q_EMIT signal_error("Error while downloading file");
    } else if (!redirectionTarget.isNull()) {
        qDebug() << __FUNCTION__ << " redirected link: " << redirectionTarget.toUrl();
        slot_downloadFile(redirectionTarget.toUrl(), m_requestedFileName);
    } else {
        qDebug() << __FUNCTION__ << "downloading of the file finished success";
        m_pb = 0;
        m_requestedFileName.clear();
        Q_EMIT signal_downloadFileFinished();
    }

    finishedReply->deleteLater();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::slot_readFile()
{
    m_file.write(m_reply->readAll());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UFileDownloader::slot_sslErrors(QNetworkReply *reply, QList<QSslError> errorList) {
    Q_UNUSED(reply)
    Q_UNUSED(errorList)
    // SSL errors are not suppressed — Qt's default validation applies.
    // ignoreSslErrors() was intentionally removed to prevent MITM attacks on update downloads.
}
