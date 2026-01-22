#!/usr/bin/env pwsh
# Revert to vanilla (unmodded) game
# Removes HeadTracking mod, and ASI Loader ONLY if we installed it
# Usage: pixi run vanilla

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$StateFileName = ".headtracking-state.json"

# Auto-detect game path
$searchPaths = @(
    'C:\Program Files (x86)\Steam\steamapps\common\Dying Light 2\ph\work\bin\x64',
    'C:\Program Files\Steam\steamapps\common\Dying Light 2\ph\work\bin\x64',
    'D:\Steam\steamapps\common\Dying Light 2\ph\work\bin\x64',
    'D:\SteamLibrary\steamapps\common\Dying Light 2\ph\work\bin\x64'
)

$gameDir = $null
foreach ($p in $searchPaths) {
    if (Test-Path $p) {
        $gameDir = $p
        break
    }
}

if (-not $gameDir) {
    Write-Host "ERROR: Game directory not found" -ForegroundColor Red
    exit 1
}

Write-Host "Reverting to vanilla (unmodded) game..." -ForegroundColor Cyan
Write-Host "  Game path: $gameDir" -ForegroundColor Gray
Write-Host ""

# Read state file
$stateFile = Join-Path $gameDir $StateFileName
$frameworkInstalledByUs = $false

if (Test-Path $stateFile) {
    try {
        $state = Get-Content $stateFile -Raw | ConvertFrom-Json
        $frameworkInstalledByUs = $state.framework.installed_by_us
        Write-Host "  Found state file - respecting installation history" -ForegroundColor Gray
    } catch {
        Write-Host "  Warning: Could not read state file, assuming full removal" -ForegroundColor Yellow
        $frameworkInstalledByUs = $true
    }
} else {
    Write-Host "  No state file found - will remove everything" -ForegroundColor Yellow
    $frameworkInstalledByUs = $true
}

$removed = $false

# Remove HeadTracking mod files
$modFiles = @("DL2HeadTracking.asi", "DL2HeadTracking.asi.bak", "HeadTracking.ini")
foreach ($file in $modFiles) {
    $path = Join-Path $gameDir $file
    if (Test-Path $path) {
        Remove-Item $path -Force
        Write-Host "  Removed: $file" -ForegroundColor Green
        $removed = $true
    }
}

# Only remove ASI loader if we installed it
if ($frameworkInstalledByUs) {
    $asiLoaderFiles = @("dinput8.dll", "version.dll", "dsound.dll", "winmm.dll")
    foreach ($file in $asiLoaderFiles) {
        $path = Join-Path $gameDir $file
        if (Test-Path $path) {
            $size = (Get-Item $path).Length
            if ($size -lt 500KB) {
                Remove-Item $path -Force
                Write-Host "  Removed: $file (ASI loader)" -ForegroundColor Green
                $removed = $true
            }
        }
    }

    # Remove any other .asi files
    Get-ChildItem -Path $gameDir -Filter "*.asi" -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-Item $_.FullName -Force
        Write-Host "  Removed: $($_.Name)" -ForegroundColor Green
        $removed = $true
    }
} else {
    Write-Host "  ASI Loader preserved (was not installed by us)" -ForegroundColor Cyan
}

# Remove state file
if (Test-Path $stateFile) {
    Remove-Item $stateFile -Force
    Write-Host "  Removed: $StateFileName" -ForegroundColor Gray
}

if (-not $removed) {
    Write-Host "  No mod files found - game is already vanilla" -ForegroundColor Yellow
}

Write-Host ""
if ($frameworkInstalledByUs) {
    Write-Host "Game is now completely vanilla (unmodded)" -ForegroundColor Cyan
} else {
    Write-Host "HeadTracking removed, ASI Loader preserved for other mods" -ForegroundColor Cyan
}
Write-Host "Use 'pixi run uninstall' to remove only HeadTracking mod" -ForegroundColor Gray
