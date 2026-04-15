/*
 * test_updater_paths.cpp
 *
 * Standalone test for UpdaterSafety::IsPathSafe and IsSourceRegularFile.
 * No Qt, no external dependencies — just Windows + std::filesystem.
 *
 * Returns 0 on success, 1 on any failure (compatible with CTest).
 */
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>

#include "../uintDebuggerUpdater/PathSafety.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
static int g_failures = 0;

#define CHECK(expr) \
    do { \
        if(!(expr)) { \
            std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #expr); \
            ++g_failures; \
        } \
    } while(0)

// ---------------------------------------------------------------------------
// IsPathSafe tests
// ---------------------------------------------------------------------------
static void TestIsPathSafe()
{
    const fs::path root = L"C:\\FakeAppRoot";

    // --- Must accept clean relative paths ---
    CHECK(UpdaterSafety::IsPathSafe(L"uintDebugger.exe",            root));
    CHECK(UpdaterSafety::IsPathSafe(L"platforms/qwindows.dll",      root));
    CHECK(UpdaterSafety::IsPathSafe(L"plugins/styles/qfusion.dll",  root));
    CHECK(UpdaterSafety::IsPathSafe(L"updater.exe",                 root));

    // --- Must reject empty path ---
    CHECK(!UpdaterSafety::IsPathSafe(L"",                           root));

    // --- Must reject absolute paths ---
    CHECK(!UpdaterSafety::IsPathSafe(L"C:\\Windows\\evil.dll",      root));
    CHECK(!UpdaterSafety::IsPathSafe(L"\\evil.dll",                 root));

    // --- Must reject any ".." component ---
    CHECK(!UpdaterSafety::IsPathSafe(L"..",                         root));
    CHECK(!UpdaterSafety::IsPathSafe(L"../evil.dll",                root));
    CHECK(!UpdaterSafety::IsPathSafe(L"../../Windows/evil.dll",     root));
    CHECK(!UpdaterSafety::IsPathSafe(L"sub/../../../evil.dll",      root));
    CHECK(!UpdaterSafety::IsPathSafe(L"a/b/../../..",               root));

    // --- Must reject paths with embedded ".." that survive lexically_normal ---
    // lexically_normal("a/../../b") => "../b"  (leading ".." preserved)
    fs::path tricky = fs::path(L"a/../../b").lexically_normal();
    CHECK(!UpdaterSafety::IsPathSafe(tricky, root));
}

// ---------------------------------------------------------------------------
// IsSourceRegularFile tests — uses real temp files on disk
// ---------------------------------------------------------------------------
static void TestIsSourceRegularFile()
{
    // Create a temporary directory for test artefacts.
    wchar_t tmpDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmpDir);
    fs::path testDir = fs::path(tmpDir) / L"uintDebuggerPathTest";
    std::error_code ec;
    fs::create_directories(testDir, ec);
    if(ec)
    {
        std::fprintf(stderr, "SKIP TestIsSourceRegularFile: cannot create temp dir\n");
        return;
    }

    // Regular file — should be accepted.
    const fs::path regularFile = testDir / L"regular.bin";
    {
        HANDLE h = CreateFileW(regularFile.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if(h != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(h, "test", 4, &written, nullptr);
            CloseHandle(h);
        }
    }
    CHECK(UpdaterSafety::IsSourceRegularFile(regularFile));

    // Directory — should be rejected.
    const fs::path subDir = testDir / L"subdir";
    fs::create_directory(subDir, ec);
    CHECK(!UpdaterSafety::IsSourceRegularFile(subDir));

    // Non-existent path — should be rejected.
    CHECK(!UpdaterSafety::IsSourceRegularFile(testDir / L"nonexistent.dll"));

    // Symlink — should be rejected (if CreateSymbolicLinkW is available/privileged).
    const fs::path symlinkPath = testDir / L"link.dll";
    fs::remove(symlinkPath, ec); // clean up any leftover
    BOOL symlinkCreated = CreateSymbolicLinkW(
        symlinkPath.c_str(), regularFile.c_str(), 0);
    if(symlinkCreated)
    {
        CHECK(!UpdaterSafety::IsSourceRegularFile(symlinkPath));
        fs::remove(symlinkPath, ec);
    }
    else
    {
        std::fprintf(stderr, "NOTE: symlink test skipped (requires SeCreateSymbolicLinkPrivilege)\n");
    }

    // Cleanup.
    fs::remove_all(testDir, ec);
}

// ---------------------------------------------------------------------------
int wmain()
{
    TestIsPathSafe();
    TestIsSourceRegularFile();

    if(g_failures == 0)
    {
        std::printf("OK — all updater path tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);
    return 1;
}
