#!/usr/bin/env pwsh
# Revert release operations

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Write-Host "Reverting release operations..." -ForegroundColor Yellow

# Remove release directory
$releaseDir = Join-Path $projectDir "release"
if (Test-Path $releaseDir) {
    Remove-Item $releaseDir -Recurse -Force
    Write-Host "  Removed: release/" -ForegroundColor Gray
}

# Clean build artifacts
$binDir = Join-Path $projectDir "bin"
$objDir = Join-Path $projectDir "obj"

if (Test-Path $binDir) {
    Remove-Item $binDir -Recurse -Force
    Write-Host "  Removed: bin/" -ForegroundColor Gray
}

if (Test-Path $objDir) {
    Remove-Item $objDir -Recurse -Force
    Write-Host "  Removed: obj/" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Release reverted. Build artifacts cleaned." -ForegroundColor Green
