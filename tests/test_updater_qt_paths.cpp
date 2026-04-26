#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include "../uintDebuggerUpdater/UpdaterWidget/uqtpathsafety.h"

static int g_failures = 0;

#define CHECK(expr) \
    do { \
        if(!(expr)) \
            ++g_failures; \
    } while(false)

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir root;
    CHECK(root.isValid());

    const QString rootPath = QDir::cleanPath(root.path());

    CHECK(UpdaterQtSafety::IsPathSafeForRoot(QStringLiteral("D3Dcompiler_47.dll"), rootPath));
    CHECK(UpdaterQtSafety::IsPathSafeForRoot(QStringLiteral("platforms/qwindows.dll"), rootPath));
    CHECK(UpdaterQtSafety::IsPathSafeForRoot(QStringLiteral("styles/qmodernwindowsstyle.dll"), rootPath));

    CHECK(!UpdaterQtSafety::IsPathSafeForRoot(QString(), rootPath));
    CHECK(!UpdaterQtSafety::IsPathSafeForRoot(QStringLiteral("../evil.dll"), rootPath));
    CHECK(!UpdaterQtSafety::IsPathSafeForRoot(QStringLiteral("a/../../evil.dll"), rootPath));
    CHECK(!UpdaterQtSafety::IsPathSafeForRoot(QStringLiteral("C:/Windows/evil.dll"), rootPath));
    CHECK(!UpdaterQtSafety::IsPathSafeForRoot(QStringLiteral("\\\\server\\share\\evil.dll"), rootPath));

    return g_failures == 0 ? 0 : 1;
}
