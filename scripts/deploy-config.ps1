#!/usr/bin/env pwsh
# Deploy only the config file to game directory (no rebuild required)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Write-Host "Deploying configuration file to Dying Light 2..." -ForegroundColor Cyan

# Detect game path
$gamePath = & "$scriptDir\detect-game.ps1"
if ($LASTEXITCODE -ne 0) {
    exit 1
}

$targetDir = Join-Path $gamePath "ph\work\bin\x64"
Write-Host "Target directory: $targetDir" -ForegroundColor Gray

# Source and target INI files
$sourceIni = Join-Path $projectDir "HeadTracking.ini"
$targetIni = Join-Path $targetDir "HeadTracking.ini"

if (-not (Test-Path $sourceIni)) {
    Write-Error "Source config not found: $sourceIni"
    exit 1
}

# Backup existing config if present
if (Test-Path $targetIni) {
    $backupIni = "$targetIni.bak"
    Copy-Item $targetIni $backupIni -Force
    Write-Host "  Backed up existing config to: $backupIni" -ForegroundColor Gray
}

# Copy INI
Copy-Item $sourceIni $targetIni -Force
Write-Host "  Copied: HeadTracking.ini" -ForegroundColor Green

Write-Host ""
Write-Host "Configuration deployed!" -ForegroundColor Green
Write-Host ""
Write-Host "Current settings:" -ForegroundColor Cyan

# Display key settings from the INI
$iniContent = Get-Content $sourceIni
$inSection = ""
$displaySections = @("Sensitivity", "Deadzone", "Smoothing")

foreach ($line in $iniContent) {
    if ($line -match '^\[([^\]]+)\]') {
        $inSection = $matches[1]
    }
    elseif ($displaySections -contains $inSection) {
        if ($line -match '^([^;=]+)=(.+)$') {
            $key = $matches[1].Trim()
            $value = $matches[2].Trim()
            Write-Host "  [$inSection] $key = $value" -ForegroundColor White
        }
    }
}

Write-Host ""
Write-Host "Note: Changes take effect after game restart or config reload (if supported)" -ForegroundColor Yellow
