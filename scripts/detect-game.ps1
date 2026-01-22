#!/usr/bin/env pwsh
# Detect Dying Light 2 installation directory

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Find-DL2Installation {
    # Check environment variable override first
    if ($env:DL2_GAME_PATH) {
        $gamePath = $env:DL2_GAME_PATH
        if (Test-DL2Installation $gamePath) {
            return $gamePath
        }
        Write-Warning "DL2_GAME_PATH is set but path is invalid: $gamePath"
    }

    # Find Steam installation
    $steamPath = $null

    # Try registry (64-bit)
    try {
        $steamPath = (Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -ErrorAction Stop).InstallPath
    } catch { }

    # Try registry (32-bit fallback)
    if (-not $steamPath) {
        try {
            $steamPath = (Get-ItemProperty -Path "HKLM:\SOFTWARE\Valve\Steam" -ErrorAction Stop).InstallPath
        } catch { }
    }

    if (-not $steamPath) {
        return $null
    }

    # Parse libraryfolders.vdf to find all Steam library paths
    $libraryFolders = @($steamPath)
    $vdfPath = Join-Path $steamPath "steamapps\libraryfolders.vdf"

    if (Test-Path $vdfPath) {
        $content = Get-Content $vdfPath -Raw
        # Match path entries in VDF format
        $matches = [regex]::Matches($content, '"path"\s+"([^"]+)"')
        foreach ($match in $matches) {
            $path = $match.Groups[1].Value -replace '\\\\', '\'
            if ($path -and (Test-Path $path)) {
                $libraryFolders += $path
            }
        }
    }

    # Known folder names for DL2 (including Reloaded Edition)
    $folderNames = @(
        "Dying Light 2 Stay Human",
        "Dying Light 2 Reloaded Edition",
        "Dying Light 2",
        "Dying Light 2 Stay Human Reloaded Edition"
    )

    # Search each library for DL2
    foreach ($library in $libraryFolders) {
        foreach ($folderName in $folderNames) {
            $gamePath = Join-Path $library "steamapps\common\$folderName"
            if (Test-DL2Installation $gamePath) {
                return $gamePath
            }
        }
    }

    return $null
}

function Test-DL2Installation {
    param([string]$path)

    if (-not (Test-Path $path)) {
        return $false
    }

    $exePath = Join-Path $path "ph\work\bin\x64\DyingLightGame_x64_rwdi.exe"
    $dllPath = Join-Path $path "ph\work\bin\x64\gamedll_ph_x64_rwdi.dll"

    return (Test-Path $exePath) -and (Test-Path $dllPath)
}

# Main
$gamePath = Find-DL2Installation

if ($gamePath) {
    # Output path to stdout for capture by other scripts
    Write-Output $gamePath
    exit 0
} else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  ERROR: Dying Light 2 not found" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""

    # Try to find Steam and show what folders exist
    $steamPath = $null
    try {
        $steamPath = (Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -ErrorAction Stop).InstallPath
    } catch { }
    if (-not $steamPath) {
        try {
            $steamPath = (Get-ItemProperty -Path "HKLM:\SOFTWARE\Valve\Steam" -ErrorAction Stop).InstallPath
        } catch { }
    }

    if ($steamPath) {
        $commonPath = Join-Path $steamPath "steamapps\common"
        if (Test-Path $commonPath) {
            $dl2Folders = Get-ChildItem $commonPath -Directory | Where-Object { $_.Name -match "Dying Light" }
            if ($dl2Folders) {
                Write-Host "Found Dying Light folders in Steam:" -ForegroundColor Yellow
                foreach ($folder in $dl2Folders) {
                    Write-Host "  - $($folder.Name)" -ForegroundColor Cyan
                }
                Write-Host ""
                Write-Host "Set the game path manually:" -ForegroundColor White
                Write-Host "  `$env:DL2_GAME_PATH = '$($dl2Folders[0].FullName)'" -ForegroundColor Green
                Write-Host "  pixi run deploy" -ForegroundColor Green
                Write-Host ""
                exit 1
            }
        }
    }

    Write-Host "Searched for these folder names:" -ForegroundColor Yellow
    Write-Host "  - Dying Light 2 Stay Human" -ForegroundColor Gray
    Write-Host "  - Dying Light 2 Reloaded Edition" -ForegroundColor Gray
    Write-Host "  - Dying Light 2" -ForegroundColor Gray
    Write-Host ""
    Write-Host "To fix this:" -ForegroundColor Yellow
    Write-Host "  1. Find your Dying Light 2 installation folder" -ForegroundColor White
    Write-Host "  2. Set the environment variable:" -ForegroundColor White
    Write-Host "     `$env:DL2_GAME_PATH = 'C:\path\to\Dying Light 2'" -ForegroundColor Green
    Write-Host "  3. Run deploy again:" -ForegroundColor White
    Write-Host "     pixi run deploy" -ForegroundColor Green
    Write-Host ""
    exit 1
}
