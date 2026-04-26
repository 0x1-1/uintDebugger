/*
 * test_updater_paths.cpp
 *
 * Standalone smoke tests for updater path validation and file apply/rollback.
 * No Qt dependency; compatible with CTest.
 */
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../uintDebuggerUpdater/PathSafety.h"
#include "../uintDebuggerUpdater/UpdaterApply.h"

namespace fs = std::filesystem;

static int g_failures = 0;

#define CHECK(expr) \
    do { \
        if(!(expr)) { \
            std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #expr); \
            ++g_failures; \
        } \
    } while(0)

static fs::path UniqueTempDir(const wchar_t *name)
{
    wchar_t tempRoot[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempRoot);
    return fs::path(tempRoot) / (std::wstring(name) + L"_" + std::to_wstring(GetCurrentProcessId()));
}

static void WriteText(const fs::path &path, const std::string &text)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

static std::string ReadText(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

static void TestIsPathSafe()
{
    const fs::path root = UniqueTempDir(L"uintDebuggerPathRoot");
    std::error_code ec;
    fs::create_directories(root, ec);

    CHECK(UpdaterSafety::IsPathSafe(L"uintDebugger.exe", root));
    CHECK(UpdaterSafety::IsPathSafe(L"platforms/qwindows.dll", root));
    CHECK(UpdaterSafety::IsPathSafe(L"plugins/styles/qfusion.dll", root));
    CHECK(UpdaterSafety::IsPathSafe(L"updater.exe", root));

    CHECK(!UpdaterSafety::IsPathSafe(L"", root));
    CHECK(!UpdaterSafety::IsPathSafe(L"C:\\Windows\\evil.dll", root));
    CHECK(!UpdaterSafety::IsPathSafe(L"\\evil.dll", root));

    CHECK(!UpdaterSafety::IsPathSafe(L"..", root));
    CHECK(!UpdaterSafety::IsPathSafe(L"../evil.dll", root));
    CHECK(!UpdaterSafety::IsPathSafe(L"../../Windows/evil.dll", root));
    CHECK(!UpdaterSafety::IsPathSafe(L"sub/../../../evil.dll", root));
    CHECK(!UpdaterSafety::IsPathSafe(L"a/b/../../..", root));

    const fs::path tricky = fs::path(L"a/../../b").lexically_normal();
    CHECK(!UpdaterSafety::IsPathSafe(tricky, root));

    fs::remove_all(root, ec);
}

static void TestReservedUpdaterPaths()
{
    CHECK(UpdaterApply::ContainsUpdaterFile(std::vector<std::wstring>{L"updater.exe"}));
    CHECK(UpdaterApply::ContainsUpdaterFile(std::vector<std::wstring>{L"tools/Updater.EXE"}));

    CHECK(UpdaterApply::IsReservedUpdaterPath(L"update_tmp.exe"));
    CHECK(UpdaterApply::IsReservedUpdaterPath(L"Update_Tmp.EXE"));
    CHECK(UpdaterApply::IsReservedUpdaterPath(L"update_backup/file.bin"));
    CHECK(UpdaterApply::IsReservedUpdaterPath(L"UPDATE_BACKUP/file.bin"));
    CHECK(!UpdaterApply::IsReservedUpdaterPath(L"uintDebugger.exe"));
}

static void TestIsSourceRegularFile()
{
    const fs::path testDir = UniqueTempDir(L"uintDebuggerSourceTest");
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    if(ec)
    {
        std::fprintf(stderr, "SKIP TestIsSourceRegularFile: cannot create temp dir\n");
        return;
    }

    const fs::path regularFile = testDir / L"regular.bin";
    WriteText(regularFile, "test");
    CHECK(UpdaterSafety::IsSourceRegularFile(regularFile));

    const fs::path subDir = testDir / L"subdir";
    fs::create_directory(subDir, ec);
    CHECK(!UpdaterSafety::IsSourceRegularFile(subDir));

    CHECK(!UpdaterSafety::IsSourceRegularFile(testDir / L"nonexistent.dll"));

    const fs::path symlinkPath = testDir / L"link.dll";
    fs::remove(symlinkPath, ec);
    const BOOL symlinkCreated = CreateSymbolicLinkW(
        symlinkPath.c_str(),
        regularFile.c_str(),
        0);
    if(symlinkCreated)
    {
        CHECK(!UpdaterSafety::IsSourceRegularFile(symlinkPath));
        fs::remove(symlinkPath, ec);
    }
    else
    {
        std::fprintf(stderr, "NOTE: symlink test skipped (requires SeCreateSymbolicLinkPrivilege)\n");
    }

    fs::remove_all(testDir, ec);
}

static void TestApplyUpdateFilesSuccess()
{
    const fs::path appDir = UniqueTempDir(L"uintDebuggerApplySuccess");
    const fs::path updatesDir = appDir / L"updates";
    std::error_code ec;
    fs::remove_all(appDir, ec);

    WriteText(appDir / L"existing.txt", "old");
    WriteText(updatesDir / L"existing.txt", "new");
    WriteText(updatesDir / L"nested/new.txt", "created");

    std::wstring error;
    const bool ok = UpdaterApply::ApplyUpdateFiles(
        appDir,
        updatesDir,
        std::vector<std::wstring>{L"existing.txt", L"nested/new.txt"},
        &error);

    CHECK(ok);
    CHECK(error.empty());
    CHECK(ReadText(appDir / L"existing.txt") == "new");
    CHECK(ReadText(appDir / L"nested/new.txt") == "created");
    CHECK(!fs::exists(UpdaterApply::BackupDirectory(appDir)));

    fs::remove_all(appDir, ec);
}

static void TestApplyUpdateFilesRejectsMissingBeforeTouch()
{
    const fs::path appDir = UniqueTempDir(L"uintDebuggerApplyMissing");
    const fs::path updatesDir = appDir / L"updates";
    std::error_code ec;
    fs::remove_all(appDir, ec);

    WriteText(appDir / L"existing.txt", "old");
    WriteText(updatesDir / L"existing.txt", "new");

    std::wstring error;
    const bool ok = UpdaterApply::ApplyUpdateFiles(
        appDir,
        updatesDir,
        std::vector<std::wstring>{L"existing.txt", L"missing.txt"},
        &error);

    CHECK(!ok);
    CHECK(error.find(L"Missing update file") != std::wstring::npos);
    CHECK(ReadText(appDir / L"existing.txt") == "old");

    fs::remove_all(appDir, ec);
}

static void TestRollbackAppliedFiles()
{
    const fs::path appDir = UniqueTempDir(L"uintDebuggerRollback");
    const fs::path backupDir = UpdaterApply::BackupDirectory(appDir);
    std::error_code ec;
    fs::remove_all(appDir, ec);

    WriteText(appDir / L"existing.txt", "new");
    WriteText(backupDir / L"existing.txt", "old");
    WriteText(appDir / L"created.txt", "created");

    const bool ok = UpdaterApply::RollbackAppliedFiles(
        appDir,
        std::vector<fs::path>{L"existing.txt", L"created.txt"});

    CHECK(ok);
    CHECK(ReadText(appDir / L"existing.txt") == "old");
    CHECK(!fs::exists(appDir / L"created.txt"));
    CHECK(!fs::exists(backupDir));

    fs::remove_all(appDir, ec);
}

int wmain()
{
    TestIsPathSafe();
    TestReservedUpdaterPaths();
    TestIsSourceRegularFile();
    TestApplyUpdateFilesSuccess();
    TestApplyUpdateFilesRejectsMissingBeforeTouch();
    TestRollbackAppliedFiles();

    if(g_failures == 0)
    {
        std::printf("OK - all updater tests passed.\n");
        return 0;
    }

    std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);
    return 1;
}
