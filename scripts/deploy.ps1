#!/usr/bin/env pwsh
# Deploy debug build to game directory

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

$configuration = if ($args[0]) { $args[0] } else { "Debug" }

Write-Host "Deploying $configuration build to Dying Light 2..." -ForegroundColor Cyan

# Detect game path
$gamePath = & "$scriptDir\detect-game.ps1"
if ($LASTEXITCODE -ne 0) {
    exit 1
}

$targetDir = Join-Path $gamePath "ph\work\bin\x64"
Write-Host "Target directory: $targetDir" -ForegroundColor Gray

# Check for ASI loader - auto-install if missing
$asiLoader = Join-Path $targetDir "winmm.dll"
if (-not (Test-Path $asiLoader)) {
    Write-Host "ASI Loader not found. Installing..." -ForegroundColor Yellow

    $asiUrl = "https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.0/Ultimate-ASI-Loader_x64.zip"
    $asiZip = Join-Path $env:TEMP "ASILoader_install.zip"

    Write-Host "  Downloading Ultimate ASI Loader (v9.7.0)..." -ForegroundColor Gray
    curl.exe -fL -o $asiZip $asiUrl
    if ($LASTEXITCODE -ne 0) {
        throw "ASI Loader download failed. Check your internet connection."
    }

    Write-Host "  Extracting to game directory..." -ForegroundColor Gray
    Expand-Archive -Path $asiZip -DestinationPath $targetDir -Force
    Remove-Item $asiZip -ErrorAction SilentlyContinue

    # Rename dinput8.dll to winmm.dll (DL2 compatibility)
    $dinput = Join-Path $targetDir "dinput8.dll"
    if (Test-Path $dinput) {
        Move-Item $dinput $asiLoader -Force
    }

    if (-not (Test-Path $asiLoader)) {
        throw "ASI Loader install failed: winmm.dll not found after extraction."
    }

    Write-Host "  Ultimate ASI Loader installed!" -ForegroundColor Green
}

# Source files
$sourceAsi = Join-Path $projectDir "bin\$configuration\DL2HeadTracking.asi"
$sourceIni = Join-Path $projectDir "HeadTracking.ini"

if (-not (Test-Path $sourceAsi)) {
    Write-Error "Build artifact not found: $sourceAsi"
    Write-Host "Run 'pixi run build' first." -ForegroundColor Yellow
    exit 1
}

# Target files
$targetAsi = Join-Path $targetDir "DL2HeadTracking.asi"
$targetIni = Join-Path $targetDir "HeadTracking.ini"

# Backup existing files
if (Test-Path $targetAsi) {
    $backupAsi = "$targetAsi.bak"
    Copy-Item $targetAsi $backupAsi -Force
    Write-Host "  Backed up existing ASI to: $backupAsi" -ForegroundColor Gray
}

# Copy ASI
Copy-Item $sourceAsi $targetAsi -Force
Write-Host "  Copied: DL2HeadTracking.asi" -ForegroundColor Green

# Copy INI only if it doesn't exist (preserve user settings)
if (-not (Test-Path $targetIni)) {
    if (Test-Path $sourceIni) {
        Copy-Item $sourceIni $targetIni -Force
        Write-Host "  Copied: HeadTracking.ini (default config)" -ForegroundColor Green
    }
} else {
    Write-Host "  Skipped: HeadTracking.ini (preserving existing config)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Deployment complete!" -ForegroundColor Green
Write-Host "Files deployed to: $targetDir" -ForegroundColor Cyan
