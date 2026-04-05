param(
    [string]$ReleaseDir = ".\Release",
    [string]$Version = "",
    [string]$Owner = "uintptr",
    [string]$Repository = "uintDebugger",
    [string]$Tag = "",
    [string]$ManifestAssetName = "uintDebugger-update-manifest.json"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ProjectVersion {
    param([string]$RepoRoot)

    $cmakePath = Join-Path $RepoRoot "CMakeLists.txt"
    $match = Select-String -Path $cmakePath -Pattern 'project\(uintDebugger VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
    if (-not $match) {
        throw "Unable to determine project version from $cmakePath"
    }

    return $match.Matches[0].Groups[1].Value
}

function Get-AssetName {
    param([string]$RelativePath)

    if ($RelativePath.Contains('/')) {
        return $RelativePath.Replace('/', '__')
    }

    return $RelativePath
}

function Get-RelativeUnixPath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $normalizedBase = [System.IO.Path]::GetFullPath($BasePath)
    if (-not $normalizedBase.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $normalizedBase += [System.IO.Path]::DirectorySeparatorChar
    }

    $baseUri = [System.Uri]::new($normalizedBase)
    $targetUri = [System.Uri]::new([System.IO.Path]::GetFullPath($TargetPath))
    return $baseUri.MakeRelativeUri($targetUri).ToString()
}

function Should-IncludeFile {
    param([System.IO.FileInfo]$File)

    $excludedNames = @(
        "uintDebugger_qt.log",
        "update_tmp.exe",
        "upload-release-assets.ps1",
        "uintDebugger-update-manifest.json"
    )

    $excludedExtensions = @(".dmp", ".ilk", ".lastbuildstate", ".log", ".pdb")

    if ($excludedNames -contains $File.Name) {
        return $false
    }

    if ($excludedExtensions -contains $File.Extension.ToLowerInvariant()) {
        return $false
    }

    if ($File.Name -like "*_crash.txt") {
        return $false
    }

    return $true
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedReleaseDir = (Resolve-Path $ReleaseDir).Path

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Get-ProjectVersion -RepoRoot $repoRoot
}

if ([string]::IsNullOrWhiteSpace($Tag)) {
    $Tag = "v$Version"
}

$stagingDir = Join-Path $resolvedReleaseDir "github-release-assets"
if (Test-Path $stagingDir) {
    Remove-Item -LiteralPath $stagingDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stagingDir | Out-Null

$manifestEntries = @()

$releaseFiles = Get-ChildItem -LiteralPath $resolvedReleaseDir -File -Recurse |
    Where-Object { Should-IncludeFile $_ }

foreach ($file in $releaseFiles) {
    $relativePath = Get-RelativeUnixPath -BasePath $resolvedReleaseDir -TargetPath $file.FullName
    $assetName = Get-AssetName -RelativePath $relativePath
    $stagedAssetPath = Join-Path $stagingDir $assetName

    Copy-Item -LiteralPath $file.FullName -Destination $stagedAssetPath -Force

    $manifestEntries += [ordered]@{
        path   = $relativePath
        asset  = $assetName
        size   = [int64]$file.Length
        sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$manifestObject = [ordered]@{
    schema     = 1
    version    = $Version
    tag        = $Tag
    owner      = $Owner
    repository = $Repository
    files      = $manifestEntries
}

$manifestPath = Join-Path $stagingDir $ManifestAssetName
$manifestObject | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding utf8

$uploadScriptPath = Join-Path $stagingDir "upload-release-assets.ps1"
$uploadCommands = @(
    '$ErrorActionPreference = "Stop"',
    ('$tag = "{0}"' -f $Tag.Trim()),
    '$assetDir = Split-Path -Parent $MyInvocation.MyCommand.Path',
    'Get-ChildItem -LiteralPath $assetDir -File | Where-Object { $_.Name -ne "upload-release-assets.ps1" } | ForEach-Object {',
    '    gh release upload $tag $_.FullName --clobber',
    '}'
)
$uploadCommands | Set-Content -LiteralPath $uploadScriptPath -Encoding utf8

Write-Host "Generated manifest staging folder: $stagingDir"
Write-Host "Manifest asset: $manifestPath"
Write-Host "Upload helper: $uploadScriptPath"
