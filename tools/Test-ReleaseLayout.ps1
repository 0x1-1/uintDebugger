param(
    [string]$ReleaseDir = ".\Release"
)

$ErrorActionPreference = "Stop"

$requiredFiles = @(
    "uintDebugger.exe",
    "updater.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "Qt6Network.dll",
    "platforms\qwindows.dll"
)

$missing = @()
foreach ($file in $requiredFiles) {
    $path = Join-Path $ReleaseDir $file
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        $missing += $file
        continue
    }

    if ((Get-Item -LiteralPath $path).Length -le 0) {
        $missing += "$file (empty)"
    }
}

if ($missing.Count -gt 0) {
    Write-Error ("Release layout is incomplete: " + ($missing -join ", "))
}

Write-Host "Release layout smoke check passed for $ReleaseDir"
