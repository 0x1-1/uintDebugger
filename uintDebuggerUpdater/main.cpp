#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

#include "UpdaterApply.h"

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

    if(!skipSelfUpdate && UpdaterApply::ContainsUpdaterFile(relativeFiles))
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
                UpdaterApply::CopyUpdateFile(downloadedUpdater, temporaryUpdater);
            }
            catch(const std::exception &ex)
            {
                return AbortUpdate(
                    appDirectory,
                    std::wstring(L"Failed to stage updater.exe: ") +
                        UpdaterApply::WidenMessage(ex.what()));
            }

            std::vector<std::wstring> forwardedArguments;
            forwardedArguments.push_back(L"--skip-self-update");
            forwardedArguments.insert(forwardedArguments.end(), relativeFiles.begin(), relativeFiles.end());

            if(LaunchDetached(temporaryUpdater, forwardedArguments))
                return 0;
        }
    }

    std::wstring applyError;
    if(!UpdaterApply::ApplyUpdateFiles(appDirectory, updatesDirectory, relativeFiles, &applyError))
        return AbortUpdate(appDirectory, applyError);

    std::error_code removeError;
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
