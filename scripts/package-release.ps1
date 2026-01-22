#!/usr/bin/env pwsh
#Requires -Version 5.1
# Custom packaging for DL2 Head Tracking (C++ project, no .csproj)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = 'SilentlyContinue'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

# Get version from manifest
$manifest = Get-Content (Join-Path $projectDir "manifest.json") | ConvertFrom-Json
$version = $manifest.version

Write-Host "=== DL2 Head Tracking - Package Release ===" -ForegroundColor Magenta
Write-Host ""
Write-Host "Version: $version" -ForegroundColor Cyan
Write-Host ""

$releaseDir = Join-Path $projectDir "release"
$stagingDir = Join-Path $releaseDir "staging"

# Create release directory
if (-not (Test-Path $releaseDir)) {
    New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
}

# Clean staging
if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

Write-Host "Staging release files..." -ForegroundColor Cyan

# Copy install/uninstall scripts
$scriptsDir = Join-Path $projectDir "scripts"
foreach ($script in @("install.cmd", "uninstall.cmd")) {
    $scriptPath = Join-Path $scriptsDir $script
    if (Test-Path $scriptPath) {
        Copy-Item $scriptPath -Destination $stagingDir -Force
        Write-Host "  $script" -ForegroundColor Green
    } else {
        throw "Required script not found: $scriptPath"
    }
}

# Copy mod files to plugins subfolder
$pluginsDir = Join-Path $stagingDir "plugins"
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null

$asiPath = Join-Path $projectDir "bin/Release/DL2HeadTracking.asi"
if (-not (Test-Path $asiPath)) {
    throw "DL2HeadTracking.asi not found at: $asiPath"
}
Copy-Item $asiPath -Destination $pluginsDir -Force
Write-Host "  plugins/DL2HeadTracking.asi" -ForegroundColor Green

$iniPath = Join-Path $projectDir "HeadTracking.ini"
if (-not (Test-Path $iniPath)) {
    throw "HeadTracking.ini not found at: $iniPath"
}
Copy-Item $iniPath -Destination $pluginsDir -Force
Write-Host "  plugins/HeadTracking.ini" -ForegroundColor Green

# Copy documentation
$docFiles = @("README.md", "LICENSE", "CHANGELOG.md", "THIRD_PARTY_LICENSES.md")
foreach ($doc in $docFiles) {
    $docPath = Join-Path $projectDir $doc
    if (Test-Path $docPath) {
        Copy-Item $docPath -Destination $stagingDir -Force
        Write-Host "  $doc" -ForegroundColor Green
    } elseif ($doc -eq "LICENSE") {
        Write-Host "  WARNING: $doc not found" -ForegroundColor Yellow
    }
}

Write-Host ""

# Create ZIP archive
$zipName = "DL2HeadTracking-v$version.zip"
$zipPath = Join-Path $releaseDir $zipName

if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Write-Host "Creating ZIP archive..." -ForegroundColor Cyan

Push-Location $stagingDir
try {
    Compress-Archive -Path ".\*" -DestinationPath $zipPath -Force
} finally {
    Pop-Location
}

Remove-Item -Recurse -Force $stagingDir

$zipSize = (Get-Item $zipPath).Length / 1KB
Write-Host ""
Write-Host "=== Package Complete ===" -ForegroundColor Magenta
Write-Host ""
Write-Host "Release archive: $zipPath" -ForegroundColor Green
Write-Host ("Size: {0:N1} KB" -f $zipSize) -ForegroundColor White

# Output zip path for CI capture
Write-Output $zipPath
