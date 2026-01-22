#!/usr/bin/env pwsh
# Validate release readiness

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Write-Host "Validating release readiness..." -ForegroundColor Cyan
Write-Host ""

$errors = @()
$warnings = @()

# Check release ASI exists
$releaseAsi = Join-Path $projectDir "bin\Release\DL2HeadTracking.asi"
if (Test-Path $releaseAsi) {
    $size = (Get-Item $releaseAsi).Length
    Write-Host "[OK] Release ASI exists ($size bytes)" -ForegroundColor Green
} else {
    $errors += "Release ASI not found. Run 'pixi run build-release' first."
}

# Check manifest.json
$manifest = Join-Path $projectDir "manifest.json"
if (Test-Path $manifest) {
    try {
        $json = Get-Content $manifest | ConvertFrom-Json
        if ($json.version) {
            Write-Host "[OK] manifest.json version: $($json.version)" -ForegroundColor Green
        } else {
            $errors += "manifest.json missing version field"
        }
    } catch {
        $errors += "manifest.json is not valid JSON"
    }
} else {
    $errors += "manifest.json not found"
}

# Check CHANGELOG.md
$changelog = Join-Path $projectDir "CHANGELOG.md"
if (Test-Path $changelog) {
    $content = Get-Content $changelog -Raw
    if ($content -match "\[Unreleased\]") {
        Write-Host "[OK] CHANGELOG.md has Unreleased section" -ForegroundColor Green
    } else {
        $warnings += "CHANGELOG.md missing [Unreleased] section"
    }
} else {
    $errors += "CHANGELOG.md not found"
}

# Check HeadTracking.ini
$ini = Join-Path $projectDir "HeadTracking.ini"
if (Test-Path $ini) {
    Write-Host "[OK] HeadTracking.ini exists" -ForegroundColor Green
} else {
    $errors += "HeadTracking.ini not found"
}

# Check README.md
$readme = Join-Path $projectDir "README.md"
if (Test-Path $readme) {
    Write-Host "[OK] README.md exists" -ForegroundColor Green
} else {
    $warnings += "README.md not found"
}

# Check LICENSE
$license = Join-Path $projectDir "LICENSE"
if (Test-Path $license) {
    Write-Host "[OK] LICENSE exists" -ForegroundColor Green
} else {
    $warnings += "LICENSE not found"
}

Write-Host ""

# Report warnings
if ($warnings.Count -gt 0) {
    Write-Host "Warnings:" -ForegroundColor Yellow
    foreach ($warning in $warnings) {
        Write-Host "  - $warning" -ForegroundColor Yellow
    }
    Write-Host ""
}

# Report errors
if ($errors.Count -gt 0) {
    Write-Host "Errors:" -ForegroundColor Red
    foreach ($error in $errors) {
        Write-Host "  - $error" -ForegroundColor Red
    }
    Write-Host ""
    Write-Error "Validation failed with $($errors.Count) error(s)"
    exit 1
}

Write-Host "Validation passed!" -ForegroundColor Green
exit 0
