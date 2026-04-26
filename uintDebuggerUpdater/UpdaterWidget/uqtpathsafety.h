#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace UpdaterQtSafety
{

inline QString NormalizedAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

inline bool IsPathSafeForRoot(const QString &relativePath, const QString &rootDir)
{
    if(relativePath.isEmpty() || QFileInfo(relativePath).isAbsolute())
        return false;

    if(relativePath.split(QLatin1Char('/')).contains(QStringLiteral("..")))
        return false;

    const QString cleanRelative = QDir::cleanPath(relativePath);
    const QString resolvedPath = NormalizedAbsolutePath(QDir(rootDir).absoluteFilePath(cleanRelative));
    const QString canonicalRoot = NormalizedAbsolutePath(QDir(rootDir).canonicalPath());
    if(canonicalRoot.isEmpty())
        return false;

    return resolvedPath == canonicalRoot ||
           resolvedPath.startsWith(canonicalRoot + QLatin1Char('/'));
}

} // namespace UpdaterQtSafety
