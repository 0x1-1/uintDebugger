/*
 * Shared updater apply/rollback helpers.
 *
 * Kept header-only so updater.exe and the smoke tests exercise the same code
 * without introducing another CMake target.
 */
#pragma once

#include <cstring>
#include <exception>
#include <filesystem>
#include <cwctype>
#include <string>
#include <vector>

#include "PathSafety.h"

namespace UpdaterApply
{

inline std::wstring ToLowerPathComponent(const std::wstring &value)
{
    std::wstring lowered;
    lowered.reserve(value.size());
    for(std::wstring::const_iterator it = value.begin(); it != value.end(); ++it)
        lowered.push_back(static_cast<wchar_t>(std::towlower(*it)));
    return lowered;
}

inline bool ContainsUpdaterFile(const std::vector<std::wstring> &relativeFiles)
{
    for(std::vector<std::wstring>::const_iterator it = relativeFiles.begin(); it != relativeFiles.end(); ++it)
    {
        if(ToLowerPathComponent(std::filesystem::path(*it).filename().wstring()) == L"updater.exe")
            return true;
    }
    return false;
}

inline bool IsReservedUpdaterPath(const std::filesystem::path &relativePath)
{
    if(relativePath.empty())
        return false;

    const std::filesystem::path normalizedPath = relativePath.lexically_normal();
    const auto component = normalizedPath.begin();
    if(component != normalizedPath.end() &&
        ToLowerPathComponent(component->wstring()) == L"update_backup")
        return true;

    return ToLowerPathComponent(normalizedPath.filename().wstring()) == L"update_tmp.exe";
}

inline std::filesystem::path BackupDirectory(const std::filesystem::path &appDirectory)
{
    return appDirectory / L"update_backup";
}

inline std::wstring WidenMessage(const char *message)
{
    if(message == nullptr)
        return L"unknown error";
    return std::wstring(message, message + std::strlen(message));
}

inline void CopyUpdateFile(
    const std::filesystem::path &sourcePath,
    const std::filesystem::path &destinationPath)
{
    if(destinationPath.has_parent_path())
        std::filesystem::create_directories(destinationPath.parent_path());

    std::filesystem::copy_file(
        sourcePath,
        destinationPath,
        std::filesystem::copy_options::overwrite_existing);
}

inline void BackupDestinationFile(
    const std::filesystem::path &destinationPath,
    const std::filesystem::path &backupPath)
{
    if(!destinationPath.has_parent_path())
        return;

    if(backupPath.has_parent_path())
        std::filesystem::create_directories(backupPath.parent_path());

    std::filesystem::copy_file(
        destinationPath,
        backupPath,
        std::filesystem::copy_options::overwrite_existing);
}

inline bool RollbackAppliedFiles(
    const std::filesystem::path &appDirectory,
    const std::vector<std::filesystem::path> &appliedFiles)
{
    const std::filesystem::path backupDirectory = BackupDirectory(appDirectory);
    std::error_code ec;

    for(std::vector<std::filesystem::path>::const_reverse_iterator it = appliedFiles.rbegin(); it != appliedFiles.rend(); ++it)
    {
        const std::filesystem::path destinationPath = appDirectory / *it;
        const std::filesystem::path backupPath = backupDirectory / *it;

        if(std::filesystem::exists(backupPath, ec))
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
            std::filesystem::remove(destinationPath, ec);
        }

        if(ec)
            return false;
    }

    ec.clear();
    std::filesystem::remove_all(backupDirectory, ec);
    return !ec;
}

inline bool ApplyUpdateFiles(
    const std::filesystem::path &appDirectory,
    const std::filesystem::path &updatesDirectory,
    const std::vector<std::wstring> &relativeFiles,
    std::wstring *errorMessage)
{
    for(std::vector<std::wstring>::const_iterator it = relativeFiles.begin(); it != relativeFiles.end(); ++it)
    {
        const std::filesystem::path relativePath = std::filesystem::path(*it).lexically_normal();
        if(!UpdaterSafety::IsPathSafe(relativePath, appDirectory))
        {
            if(errorMessage != nullptr)
                *errorMessage = std::wstring(L"Unsafe update path blocked: ") + relativePath.wstring();
            return false;
        }

        if(IsReservedUpdaterPath(relativePath))
        {
            if(errorMessage != nullptr)
                *errorMessage = std::wstring(L"Reserved updater path blocked: ") + relativePath.wstring();
            return false;
        }

        const std::filesystem::path sourcePath = updatesDirectory / relativePath;
        if(!std::filesystem::exists(sourcePath))
        {
            if(errorMessage != nullptr)
                *errorMessage = std::wstring(L"Missing update file: ") + relativePath.wstring();
            return false;
        }

        if(!UpdaterSafety::IsSourceRegularFile(sourcePath))
        {
            if(errorMessage != nullptr)
                *errorMessage = std::wstring(L"Refused to apply non-regular update file: ") + relativePath.wstring();
            return false;
        }

        const std::filesystem::path destinationPath = appDirectory / relativePath;
        if(std::filesystem::exists(destinationPath) && !UpdaterSafety::IsSourceRegularFile(destinationPath))
        {
            if(errorMessage != nullptr)
                *errorMessage = std::wstring(L"Refused to overwrite non-regular existing file: ") + relativePath.wstring();
            return false;
        }
    }

    const std::filesystem::path backupDirectory = BackupDirectory(appDirectory);
    std::error_code cleanupError;
    std::filesystem::remove_all(backupDirectory, cleanupError);
    if(cleanupError)
    {
        if(errorMessage != nullptr)
            *errorMessage = std::wstring(L"Failed to clean updater backup directory: ") + backupDirectory.wstring();
        return false;
    }

    std::vector<std::filesystem::path> appliedFiles;
    for(std::vector<std::wstring>::const_iterator it = relativeFiles.begin(); it != relativeFiles.end(); ++it)
    {
        const std::filesystem::path relativePath = std::filesystem::path(*it).lexically_normal();
        const std::filesystem::path sourcePath = updatesDirectory / relativePath;
        const std::filesystem::path destinationPath = appDirectory / relativePath;
        const std::filesystem::path backupPath = backupDirectory / relativePath;

        bool currentNeedsRollback = !std::filesystem::exists(destinationPath);
        try
        {
            if(std::filesystem::exists(destinationPath))
            {
                BackupDestinationFile(destinationPath, backupPath);
                currentNeedsRollback = true;
            }

            CopyUpdateFile(sourcePath, destinationPath);
            appliedFiles.push_back(relativePath);
        }
        catch(const std::exception &ex)
        {
            std::vector<std::filesystem::path> rollbackFiles = appliedFiles;
            if(currentNeedsRollback)
                rollbackFiles.push_back(relativePath);

            const bool rollbackOk = RollbackAppliedFiles(appDirectory, rollbackFiles);
            if(errorMessage != nullptr)
            {
                *errorMessage = std::wstring(L"Failed to apply update file: ") +
                    relativePath.wstring() +
                    L"\n\nReason: " +
                    WidenMessage(ex.what()) +
                    (rollbackOk ? L"" : L"\n\nRollback also failed; some files may need manual repair.");
            }
            return false;
        }
    }

    std::error_code removeError;
    std::filesystem::remove_all(backupDirectory, removeError);
    if(removeError)
    {
        if(errorMessage != nullptr)
            *errorMessage = std::wstring(L"Failed to remove updater backup directory: ") + backupDirectory.wstring();
        return false;
    }

    return true;
}

} // namespace UpdaterApply
