/*
 * PathSafety.h — shared path-traversal validation for the updater.
 *
 * Included by both main.cpp (runtime) and the test suite, so every
 * function is inline / header-only.  No Qt, no external dependencies.
 */
#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace UpdaterSafety
{

// Returns true only if relativePath is safe to use as an update destination:
//   1. Not empty.
//   2. Not absolute.
//   3. Contains no ".." component anywhere (prevents a/../../evil).
//   4. When joined to appDirectory, the canonicalised result is still
//      inside appDirectory (defence-in-depth against edge cases that
//      slip through the component check).
inline bool IsPathSafe(const fs::path &relativePath, const fs::path &appDirectory)
{
    if(relativePath.empty())
        return false;

    if(relativePath.is_absolute())
        return false;

    // Walk every component; reject any that equals "..".
    for(const auto &component : relativePath)
    {
        if(component == L"..")
            return false;
    }

    // Canonicalise the full candidate path and confirm it sits inside
    // appDirectory.  weakly_canonical is used because the destination
    // file may not exist yet.
    std::error_code ec;
    const fs::path canonicalApp  = fs::weakly_canonical(appDirectory, ec);
    if(ec) return false;

    const fs::path canonicalDest = fs::weakly_canonical(appDirectory / relativePath, ec);
    if(ec) return false;

    // canonicalDest must be equal to canonicalApp or be a descendant of it.
    auto [mApp, mDest] = std::mismatch(
        canonicalApp.begin(),  canonicalApp.end(),
        canonicalDest.begin(), canonicalDest.end());

    return mApp == canonicalApp.end();
}

// Returns true only if sourcePath is a regular file (not a symlink,
// directory, or other special type).  A symlink in the updates/ directory
// could point anywhere on the filesystem, bypassing the destination check.
inline bool IsSourceRegularFile(const fs::path &sourcePath)
{
    std::error_code ec;
    // symlink_status does NOT follow symlinks — we see the link itself.
    const fs::file_status status = fs::symlink_status(sourcePath, ec);
    if(ec) return false;
    return fs::is_regular_file(status);   // rejects symlinks explicitly
}

} // namespace UpdaterSafety
