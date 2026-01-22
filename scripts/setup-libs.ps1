#!/usr/bin/env pwsh
# Setup script for DL2 Head Tracking dependencies
# Downloads MinHook source and inih for compilation with the project

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Write-Host ""
Write-Host "Setting up DL2 Head Tracking dependencies..." -ForegroundColor Cyan
Write-Host ""

# Create directories
$externDir = Join-Path $projectDir "extern"
$minhookDir = Join-Path $externDir "minhook"
$minhookSrcDir = Join-Path $minhookDir "src"

if (-not (Test-Path $externDir)) { New-Item -ItemType Directory -Path $externDir | Out-Null }
if (-not (Test-Path $minhookDir)) { New-Item -ItemType Directory -Path $minhookDir | Out-Null }
if (-not (Test-Path $minhookSrcDir)) { New-Item -ItemType Directory -Path $minhookSrcDir | Out-Null }

# MinHook v1.3.4 - Download SOURCE for compilation (ensures VS version compatibility)
$minhookIncludeDir = Join-Path $minhookDir "include"
$minhookHeader = Join-Path $minhookIncludeDir "MinHook.h"
$minhookHde = Join-Path $minhookSrcDir "hde"

if (-not (Test-Path $minhookIncludeDir)) { New-Item -ItemType Directory -Path $minhookIncludeDir | Out-Null }

# Files we need from MinHook source
$minhookFiles = @{
    "include/MinHook.h" = $minhookHeader
    "src/buffer.h" = (Join-Path $minhookSrcDir "buffer.h")
    "src/buffer.c" = (Join-Path $minhookSrcDir "buffer.c")
    "src/hook.c" = (Join-Path $minhookSrcDir "hook.c")
    "src/trampoline.h" = (Join-Path $minhookSrcDir "trampoline.h")
    "src/trampoline.c" = (Join-Path $minhookSrcDir "trampoline.c")
    "src/hde/hde32.h" = (Join-Path $minhookSrcDir "hde\hde32.h")
    "src/hde/hde32.c" = (Join-Path $minhookSrcDir "hde\hde32.c")
    "src/hde/hde64.h" = (Join-Path $minhookSrcDir "hde\hde64.h")
    "src/hde/hde64.c" = (Join-Path $minhookSrcDir "hde\hde64.c")
    "src/hde/pstdint.h" = (Join-Path $minhookSrcDir "hde\pstdint.h")
    "src/hde/table32.h" = (Join-Path $minhookSrcDir "hde\table32.h")
    "src/hde/table64.h" = (Join-Path $minhookSrcDir "hde\table64.h")
}

$baseUrl = "https://raw.githubusercontent.com/TsudaKageyu/minhook/v1.3.4"

# Check if MinHook source already downloaded
$minhookComplete = $true
foreach ($file in $minhookFiles.Values) {
    if (-not (Test-Path $file)) {
        $minhookComplete = $false
        break
    }
}

if ($minhookComplete) {
    Write-Host "[1/2] MinHook v1.3.4 source already installed" -ForegroundColor Green
} else {
    Write-Host "[1/2] Downloading MinHook v1.3.4 source..." -ForegroundColor Yellow

    # Create hde subdirectory
    $hdeDir = Join-Path $minhookSrcDir "hde"
    if (-not (Test-Path $hdeDir)) { New-Item -ItemType Directory -Path $hdeDir | Out-Null }

    $downloadErrors = @()
    foreach ($entry in $minhookFiles.GetEnumerator()) {
        $srcPath = $entry.Key
        $destPath = $entry.Value
        $url = "$baseUrl/$srcPath"
        $fileName = Split-Path -Leaf $srcPath

        try {
            Write-Host "  Downloading $fileName..." -ForegroundColor Gray
            Invoke-WebRequest -Uri $url -OutFile $destPath -UseBasicParsing
        } catch {
            $downloadErrors += "  - $fileName : $_"
        }
    }

    if ($downloadErrors.Count -gt 0) {
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Red
        Write-Host "  ERROR: Failed to download MinHook" -ForegroundColor Red
        Write-Host "========================================" -ForegroundColor Red
        Write-Host ""
        Write-Host "Download errors:" -ForegroundColor Yellow
        $downloadErrors | ForEach-Object { Write-Host $_ -ForegroundColor Red }
        Write-Host ""
        Write-Host "MANUAL FIX:" -ForegroundColor Cyan
        Write-Host "  1. Go to: https://github.com/TsudaKageyu/minhook/releases/tag/v1.3.4" -ForegroundColor White
        Write-Host "  2. Download the source code (zip)" -ForegroundColor White
        Write-Host "  3. Extract and copy these folders:" -ForegroundColor White
        Write-Host "     - include/MinHook.h -> $minhookHeader" -ForegroundColor Yellow
        Write-Host "     - src/* -> $minhookSrcDir" -ForegroundColor Yellow
        Write-Host "  4. Re-run: pixi run setup" -ForegroundColor Green
        Write-Host ""
        exit 1
    }

    Write-Host "  MinHook v1.3.4 source installed" -ForegroundColor Green
}

# inih r58
$inihHeaderUrl = "https://raw.githubusercontent.com/benhoyt/inih/r58/ini.h"
$inihSourceUrl = "https://raw.githubusercontent.com/benhoyt/inih/r58/ini.c"
$inihHeader = Join-Path $externDir "ini.h"
$inihSource = Join-Path $externDir "ini.c"

# Check if we need to download fresh copies
$needsInih = $false
if (Test-Path $inihHeader) {
    $content = Get-Content $inihHeader -Raw
    if ($content -notmatch "ini_parse_stream") {
        $needsInih = $true
    }
} else {
    $needsInih = $true
}

if ($needsInih) {
    Write-Host "[2/2] Downloading inih r58..." -ForegroundColor Yellow
    try {
        Invoke-WebRequest -Uri $inihHeaderUrl -OutFile $inihHeader -UseBasicParsing
        Write-Host "  Downloaded ini.h" -ForegroundColor Gray
        Invoke-WebRequest -Uri $inihSourceUrl -OutFile $inihSource -UseBasicParsing
        Write-Host "  Downloaded ini.c" -ForegroundColor Gray
        Write-Host "  inih r58 installed" -ForegroundColor Green
    } catch {
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Red
        Write-Host "  ERROR: Failed to download inih" -ForegroundColor Red
        Write-Host "========================================" -ForegroundColor Red
        Write-Host ""
        Write-Host "Error: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "MANUAL FIX:" -ForegroundColor Cyan
        Write-Host "  1. Download ini.h from: $inihHeaderUrl" -ForegroundColor White
        Write-Host "     Save to: $inihHeader" -ForegroundColor Yellow
        Write-Host "  2. Download ini.c from: $inihSourceUrl" -ForegroundColor White
        Write-Host "     Save to: $inihSource" -ForegroundColor Yellow
        Write-Host "  3. Re-run: pixi run setup" -ForegroundColor Green
        Write-Host ""
        exit 1
    }
} else {
    Write-Host "[2/2] inih r58 already installed" -ForegroundColor Green
}

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Cyan

$allGood = $true
$missing = @()

# Check MinHook files
if (Test-Path $minhookHeader) {
    Write-Host "  [OK] MinHook.h" -ForegroundColor Green
} else {
    Write-Host "  [MISSING] MinHook.h" -ForegroundColor Red
    $missing += @{Name = "MinHook.h"; Path = $minhookHeader; Url = "$baseUrl/include/MinHook.h"}
    $allGood = $false
}

$hookC = Join-Path $minhookSrcDir "hook.c"
if (Test-Path $hookC) {
    Write-Host "  [OK] MinHook source (hook.c)" -ForegroundColor Green
} else {
    Write-Host "  [MISSING] MinHook source" -ForegroundColor Red
    $missing += @{Name = "hook.c"; Path = $hookC; Url = "$baseUrl/src/hook.c"}
    $allGood = $false
}

# Check inih files
if (Test-Path $inihHeader) {
    Write-Host "  [OK] inih header (ini.h)" -ForegroundColor Green
} else {
    Write-Host "  [MISSING] inih header" -ForegroundColor Red
    $missing += @{Name = "ini.h"; Path = $inihHeader; Url = $inihHeaderUrl}
    $allGood = $false
}

if (Test-Path $inihSource) {
    Write-Host "  [OK] inih source (ini.c)" -ForegroundColor Green
} else {
    Write-Host "  [MISSING] inih source" -ForegroundColor Red
    $missing += @{Name = "ini.c"; Path = $inihSource; Url = $inihSourceUrl}
    $allGood = $false
}

Write-Host ""
if ($allGood) {
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  Setup complete!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Dependencies installed:" -ForegroundColor Cyan
    Write-Host "  - MinHook v1.3.4 (source - compiles with your VS version)" -ForegroundColor Gray
    Write-Host "  - inih r58" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Next step: pixi run build" -ForegroundColor Yellow
} else {
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  SETUP INCOMPLETE" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Missing files need manual download:" -ForegroundColor Yellow
    Write-Host ""

    $stepNum = 1
    foreach ($item in $missing) {
        Write-Host "[Step $stepNum] Download $($item.Name):" -ForegroundColor Cyan
        Write-Host "  URL:  $($item.Url)" -ForegroundColor White
        Write-Host "  Save: $($item.Path)" -ForegroundColor Yellow
        Write-Host ""
        $stepNum++
    }

    Write-Host "After downloading, re-run:" -ForegroundColor Cyan
    Write-Host "  pixi run setup" -ForegroundColor Green
    Write-Host ""
    exit 1
}
