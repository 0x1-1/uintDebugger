param(
    [string]$Version = "",
    [string]$Owner = "0x1-1",
    [string]$Repository = "uintDebugger",
    [string]$Tag = "",
    [string]$BuildDir = ".\Build",
    [string]$ReleaseDir = ".\Release",
    [string]$Configuration = "Release",
    [string]$QtPrefixPath = "",
    [string]$CMakePath = "",
    [switch]$Publish,
    [switch]$AllowDirty
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Resolve-CMake {
    param([string]$ExplicitPath)

    if (![string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (!(Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "CMakePath does not exist: $ExplicitPath"
        }
        return $ExplicitPath
    }

    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw "Unable to find cmake.exe. Pass -CMakePath explicitly."
}

function Resolve-CTest {
    param([string]$CMakeExe)

    $ctestPath = Join-Path (Split-Path -Parent $CMakeExe) "ctest.exe"
    if (Test-Path -LiteralPath $ctestPath -PathType Leaf) {
        return $ctestPath
    }

    $cmd = Get-Command ctest -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    throw "Unable to find ctest.exe next to cmake.exe."
}

function Get-ProjectVersion {
    param([string]$RepoRoot)

    $cmakePath = Join-Path $RepoRoot "CMakeLists.txt"
    $match = Select-String -Path $cmakePath -Pattern 'project\(uintDebugger VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
    if (!$match) {
        throw "Unable to determine project version from $cmakePath"
    }

    return $match.Matches[0].Groups[1].Value
}

function Get-GitCommit {
    param([string]$RepoRoot)

    $commit = (& git -C $RepoRoot rev-parse --short=12 HEAD 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commit)) {
        return "unknown"
    }

    return $commit.Trim()
}

function Test-GitDirty {
    param([string]$RepoRoot)

    $status = (& git -C $RepoRoot status --porcelain)
    return ![string]::IsNullOrWhiteSpace(($status -join "`n"))
}

function Invoke-Checked {
    param(
        [string]$Label,
        [scriptblock]$Command
    )

    Write-Host ""
    Write-Host "==> $Label"
    & $Command
}

$repoRoot = Get-RepoRoot
$cmakeExe = Resolve-CMake -ExplicitPath $CMakePath
$ctestExe = Resolve-CTest -CMakeExe $cmakeExe
$projectVersion = Get-ProjectVersion -RepoRoot $repoRoot

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $projectVersion
}

if ($Version -ne $projectVersion) {
    throw "Requested version $Version does not match CMake project version $projectVersion. Bump CMakeLists.txt first."
}

if ([string]::IsNullOrWhiteSpace($Tag)) {
    $Tag = "v$Version"
}

$dirty = Test-GitDirty -RepoRoot $repoRoot
if ($Publish -and $dirty -and !$AllowDirty) {
    throw "Working tree is dirty. Commit changes before publishing, or pass -AllowDirty for a local-only test release."
}

$commit = Get-GitCommit -RepoRoot $repoRoot
if ($dirty) {
    $commit = "$commit-dirty"
}

$configureArgs = @("-S", $repoRoot, "-B", (Join-Path $repoRoot $BuildDir))
if (![string]::IsNullOrWhiteSpace($QtPrefixPath)) {
    $configureArgs += "-DCMAKE_PREFIX_PATH=$QtPrefixPath"
}

Invoke-Checked "Configure CMake" {
    & $cmakeExe @configureArgs
}

Invoke-Checked "Build $Configuration" {
    & $cmakeExe --build (Join-Path $repoRoot $BuildDir) --config $Configuration --parallel
}

Invoke-Checked "Run CTest" {
    & $ctestExe --test-dir (Join-Path $repoRoot $BuildDir) --build-config $Configuration --output-on-failure
}

Invoke-Checked "Verify portable layout" {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repoRoot "tools\Test-ReleaseLayout.ps1") -ReleaseDir (Join-Path $repoRoot $ReleaseDir)
}

Invoke-Checked "Generate update manifest and release assets" {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repoRoot "tools\New-UpdateManifest.ps1") `
        -ReleaseDir (Join-Path $repoRoot $ReleaseDir) `
        -Version $Version `
        -Owner $Owner `
        -Repository $Repository `
        -Tag $Tag `
        -Commit $commit
}

$assetDir = Join-Path (Join-Path $repoRoot $ReleaseDir) "github-release-assets"

if (!$Publish) {
    Write-Host ""
    Write-Host "Release assets are ready: $assetDir"
    Write-Host "Build version: $Version+$commit"
    Write-Host "Publish with:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File .\tools\Invoke-UintDebuggerRelease.ps1 -Publish"
    exit 0
}

Invoke-Checked "Push current HEAD" {
    & git -C $repoRoot push origin HEAD
}

$tagExists = $false
& git -C $repoRoot rev-parse -q --verify "refs/tags/$Tag" *> $null
if ($LASTEXITCODE -eq 0) {
    $tagExists = $true
}

if (!$tagExists) {
    Invoke-Checked "Create local tag $Tag" {
        & git -C $repoRoot tag $Tag
    }
}

Invoke-Checked "Push tag $Tag" {
    & git -C $repoRoot push origin $Tag
}

Write-Host ""
Write-Host "Pushed $Tag for $Owner/$Repository ($Version+$commit)"
Write-Host "GitHub Actions release workflow will build, test, generate the manifest, and upload assets."
