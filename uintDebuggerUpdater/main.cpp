#include <windows.h>

#include <filesystem>
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

void CopyUpdateFile(const fs::path &sourcePath, const fs::path &destinationPath)
{
    if(destinationPath.has_parent_path())
        fs::create_directories(destinationPath.parent_path());

    fs::copy_file(sourcePath, destinationPath, fs::copy_options::overwrite_existing);
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
        if(fs::exists(downloadedUpdater) && UpdaterSafety::IsSourceRegularFile(downloadedUpdater))
        {
            CopyUpdateFile(downloadedUpdater, temporaryUpdater);

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
            return 1; // hostile/malformed manifest — abort everything
    }

    // Second pass: apply validated files.
    for(std::vector<std::wstring>::const_iterator it = relativeFiles.begin(); it != relativeFiles.end(); ++it)
    {
        const fs::path relativePath = fs::path(*it).lexically_normal();
        const fs::path sourcePath = updatesDirectory / relativePath;
        const fs::path destinationPath = appDirectory / relativePath;

        if(!fs::exists(sourcePath))
            continue;

        // Refuse to copy symlinks from the updates directory — a symlink
        // could point anywhere on the filesystem, bypassing the destination
        // path check performed above.
        if(!UpdaterSafety::IsSourceRegularFile(sourcePath))
            return 1;

        CopyUpdateFile(sourcePath, destinationPath);
    }

    std::error_code removeError;
    fs::remove_all(updatesDirectory, removeError);

    LaunchDetached(appDirectory / L"uintDebugger.exe", std::vector<std::wstring>());
    return 0;
}
