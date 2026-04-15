#include <windows.h>

#include <cstring>
#include <exception>
#include <filesystem>
#include <system_error>
#include <string>
#include <vector>

#include "PathSafety.h"

namespace fs = std::filesystem;

namespace
{
fs::path AppDirectory()
{
    wchar_t modulePath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    return fs::path(modulePath).parent_path();
}

bool LaunchDetached(const fs::path &executablePath, const std::vector<std::wstring> &arguments)
{
    std::wstring commandLine = L"\"" + executablePath.wstring() + L"\"";
    for(std::vector<std::wstring>::const_iterator it = arguments.begin(); it != arguments.end(); ++it)
        commandLine += L" \"" + *it + L"\"";

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo = {};
    std::wstring mutableCommandLine = commandLine;
    const BOOL created = CreateProcessW(
        executablePath.c_str(),
        mutableCommandLine.data(),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        executablePath.parent_path().c_str(),
        &startupInfo,
        &processInfo);

    if(created)
    {
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    }

    return created == TRUE;
}

std::vector<std::wstring> CollectArguments(int argc, wchar_t **argv, int firstIndex)
{
    std::vector<std::wstring> arguments;
    for(int index = firstIndex; index < argc; ++index)
        arguments.push_back(argv[index]);
    return arguments;
}

bool ContainsUpdaterFile(const std::vector<std::wstring> &relativeFiles)
{
    for(std::vector<std::wstring>::const_iterator it = relativeFiles.begin(); it != relativeFiles.end(); ++it)
    {
        if(fs::path(*it).filename().wstring() == L"updater.exe")
            return true;
    }
    return false;
}

bool IsReservedUpdaterPath(const fs::path &relativePath)
{
    if(relativePath.empty())
        return false;

    const fs::path normalizedPath = relativePath.lexically_normal();
    const auto component = normalizedPath.begin();
    if(component != normalizedPath.end() && *component == L"update_backup")
        return true;

    return normalizedPath.filename() == L"update_tmp.exe";
}

void CopyUpdateFile(const fs::path &sourcePath, const fs::path &destinationPath)
{
    if(destinationPath.has_parent_path())
        fs::create_directories(destinationPath.parent_path());

    fs::copy_file(sourcePath, destinationPath, fs::copy_options::overwrite_existing);
}

fs::path BackupDirectory(const fs::path &appDirectory)
{
    return appDirectory / L"update_backup";
}

std::wstring WidenMessage(const char *message)
{
    if(message == NULL)
        return L"unknown error";
    return std::wstring(message, message + strlen(message));
}

void BackupDestinationFile(const fs::path &destinationPath, const fs::path &backupPath)
{
    if(!destinationPath.has_parent_path())
        return;

    if(backupPath.has_parent_path())
        fs::create_directories(backupPath.parent_path());

    fs::copy_file(destinationPath, backupPath, fs::copy_options::overwrite_existing);
}

bool RollbackAppliedFiles(const fs::path &appDirectory, const std::vector<fs::path> &appliedFiles)
{
    const fs::path backupDirectory = BackupDirectory(appDirectory);
    std::error_code ec;

    for(std::vector<fs::path>::const_reverse_iterator it = appliedFiles.rbegin(); it != appliedFiles.rend(); ++it)
    {
        const fs::path destinationPath = appDirectory / *it;
        const fs::path backupPath = backupDirectory / *it;

        if(fs::exists(backupPath, ec))
        {
            try
            {
                CopyUpdateFile(backupPath, destinationPath);
            }
            catch(const std::exception &)
            {
                return false;
            }
        }
        else
        {
            ec.clear();
            fs::remove(destinationPath, ec);
        }

        if(ec)
            return false;
    }

    ec.clear();
    fs::remove_all(backupDirectory, ec);
    return !ec;
}

int AbortUpdate(const fs::path &appDirectory, const std::wstring &message)
{
    MessageBoxW(
        NULL,
        message.c_str(),
        L"uintDebugger updater",
        MB_OK | MB_ICONERROR);

    LaunchDetached(appDirectory / L"uintDebugger.exe", std::vector<std::wstring>());
    return 1;
}
}

int wmain(int argc, wchar_t **argv)
{
    const fs::path appDirectory = AppDirectory();
    const fs::path updatesDirectory = appDirectory / L"updates";
    const fs::path currentExecutable = fs::path(argv[0]).filename();
    const fs::path temporaryUpdater = appDirectory / L"update_tmp.exe";

    fs::current_path(appDirectory);

    if(currentExecutable != temporaryUpdater.filename())
    {
        std::error_code removeError;
        fs::remove(temporaryUpdater, removeError);
    }

    bool skipSelfUpdate = false;
    int firstFileIndex = 1;
    if(argc > 1 && std::wstring(argv[1]) == L"--skip-self-update")
    {
        skipSelfUpdate = true;
        firstFileIndex = 2;
    }

    const std::vector<std::wstring> relativeFiles = CollectArguments(argc, argv, firstFileIndex);

    if(!skipSelfUpdate && ContainsUpdaterFile(relativeFiles))
    {
        const fs::path downloadedUpdater = updatesDirectory / L"updater.exe";
        if(fs::exists(downloadedUpdater))
        {
            if(!UpdaterSafety::IsSourceRegularFile(downloadedUpdater))
            {
                return AbortUpdate(
                    appDirectory,
                    L"The downloaded updater is not a regular file. The update was cancelled.");
            }

            try
            {
                CopyUpdateFile(downloadedUpdater, temporaryUpdater);
            }
            catch(const std::exception &ex)
            {
                return AbortUpdate(
                    appDirectory,
                    std::wstring(L"Failed to stage updater.exe: ") +
                        WidenMessage(ex.what()));
            }

            std::vector<std::wstring> forwardedArguments;
            forwardedArguments.push_back(L"--skip-self-update");
            forwardedArguments.insert(forwardedArguments.end(), relativeFiles.begin(), relativeFiles.end());

            if(LaunchDetached(temporaryUpdater, forwardedArguments))
                return 0;
        }
    }

    // Validate ALL paths before touching the filesystem.  Any bad entry
    // aborts the entire update — a partial apply is worse than no apply.
    for(std::vector<std::wstring>::const_iterator it = relativeFiles.begin(); it != relativeFiles.end(); ++it)
    {
        const fs::path relativePath = fs::path(*it).lexically_normal();
        if(!UpdaterSafety::IsPathSafe(relativePath, appDirectory))
        {
            return AbortUpdate(
                appDirectory,
                std::wstring(L"Unsafe update path blocked: ") + relativePath.wstring());
        }

        if(IsReservedUpdaterPath(relativePath))
        {
            return AbortUpdate(
                appDirectory,
                std::wstring(L"Reserved updater path blocked: ") + relativePath.wstring());
        }
    }

    // Second pass: apply validated files.
    const fs::path backupDirectory = BackupDirectory(appDirectory);
    std::error_code backupCleanupError;
    fs::remove_all(backupDirectory, backupCleanupError);
    backupCleanupError.clear();

    std::vector<fs::path> appliedFiles;
    for(std::vector<std::wstring>::const_iterator it = relativeFiles.begin(); it != relativeFiles.end(); ++it)
    {
        const fs::path relativePath = fs::path(*it).lexically_normal();
        const fs::path sourcePath = updatesDirectory / relativePath;
        const fs::path destinationPath = appDirectory / relativePath;
        const fs::path backupPath = backupDirectory / relativePath;

        if(!fs::exists(sourcePath))
            continue;

        // Refuse to copy symlinks from the updates directory — a symlink
        // could point anywhere on the filesystem, bypassing the destination
        // path check performed above.
        if(!UpdaterSafety::IsSourceRegularFile(sourcePath))
        {
            return AbortUpdate(
                appDirectory,
                std::wstring(L"Refused to apply non-regular update file: ") + relativePath.wstring());
        }

        if(fs::exists(destinationPath) && !UpdaterSafety::IsSourceRegularFile(destinationPath))
        {
            const bool rollbackOk = RollbackAppliedFiles(appDirectory, appliedFiles);
            return AbortUpdate(
                appDirectory,
                std::wstring(L"Refused to overwrite non-regular existing file: ") +
                    relativePath.wstring() +
                    (rollbackOk ? L"" : L"\n\nRollback also failed; some files may need manual repair."));
        }

        try
        {
            if(fs::exists(destinationPath))
                BackupDestinationFile(destinationPath, backupPath);

            CopyUpdateFile(sourcePath, destinationPath);
            appliedFiles.push_back(relativePath);
        }
        catch(const std::exception &ex)
        {
            const bool rollbackOk = RollbackAppliedFiles(appDirectory, appliedFiles);
            return AbortUpdate(
                appDirectory,
                std::wstring(L"Failed to apply update file: ") +
                    relativePath.wstring() +
                    L"\n\nReason: " +
                    WidenMessage(ex.what()) +
                    (rollbackOk ? L"" : L"\n\nRollback also failed; some files may need manual repair."));
        }
    }

    std::error_code removeError;
    fs::remove_all(backupDirectory, removeError);
    removeError.clear();
    fs::remove_all(updatesDirectory, removeError);

    if(!LaunchDetached(appDirectory / L"uintDebugger.exe", std::vector<std::wstring>()))
    {
        MessageBoxW(
            NULL,
            L"The update finished, but uintDebugger.exe could not be started automatically.",
            L"uintDebugger updater",
            MB_OK | MB_ICONWARNING);
        return 1;
    }

    return 0;
}
