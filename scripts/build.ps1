#!/usr/bin/env pwsh
# Build script with automatic Visual Studio environment detection
# Auto-detects VS installation and sets up build environment

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$solutionFile = Join-Path $projectDir "DL2HeadTracking.sln"

Write-Host ""
Write-Host "Building DL2 Head Tracking ($Configuration)..." -ForegroundColor Cyan
Write-Host ""

# Check if msbuild is already available
$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue

if (-not $msbuild) {
    Write-Host "MSBuild not in PATH, searching for Visual Studio..." -ForegroundColor Yellow

    # Search for Visual Studio installations
    $vcvarsPaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    )

    $vcvarsPath = $null
    foreach ($path in $vcvarsPaths) {
        if (Test-Path $path) {
            $vcvarsPath = $path
            break
        }
    }

    if ($vcvarsPath) {
        # Detect VS version from path
        $vsVersion = "unknown"
        $platformToolset = ""
        if ($vcvarsPath -match "2022") {
            $vsVersion = "2022"
            $platformToolset = "v143"
        } elseif ($vcvarsPath -match "2019") {
            $vsVersion = "2019"
            $platformToolset = "v142"
        } elseif ($vcvarsPath -match "2017") {
            $vsVersion = "2017"
            $platformToolset = "v141"
        }

        Write-Host "Found Visual Studio $vsVersion" -ForegroundColor Green
        Write-Host "  Path: $vcvarsPath" -ForegroundColor Gray
        Write-Host "  Platform Toolset: $platformToolset" -ForegroundColor Gray
        Write-Host "Initializing build environment..." -ForegroundColor Gray

        # Run vcvars64.bat and capture the environment, then run msbuild
        # We need to do this in a single cmd process since environment changes don't persist
        # Override PlatformToolset to match the installed VS version
        $cmdScript = @"
@echo off
call "$vcvarsPath" >nul 2>&1
if errorlevel 1 exit /b 1
msbuild "$solutionFile" /p:Configuration=$Configuration /p:Platform=x64 /p:PlatformToolset=$platformToolset /m /nologo /v:minimal
"@
        $tempBat = Join-Path $env:TEMP "dl2_build_$([guid]::NewGuid().ToString('N')).bat"
        $cmdScript | Out-File -FilePath $tempBat -Encoding ASCII

        try {
            & cmd /c $tempBat
            $buildResult = $LASTEXITCODE
        } finally {
            Remove-Item $tempBat -Force -ErrorAction SilentlyContinue
        }

        if ($buildResult -ne 0) {
            Write-Host ""
            Write-Host "========================================" -ForegroundColor Red
            Write-Host "  BUILD FAILED" -ForegroundColor Red
            Write-Host "========================================" -ForegroundColor Red
            Write-Host ""
            Write-Host "Check the errors above. Common issues:" -ForegroundColor Yellow
            Write-Host "  - Missing Windows SDK: Install via Visual Studio Installer" -ForegroundColor White
            Write-Host "  - Missing dependencies: Run 'pixi run setup'" -ForegroundColor White
            Write-Host "  - Syntax errors: Check the source file and line shown above" -ForegroundColor White
            Write-Host ""
            exit 1
        }

        Write-Host ""
        Write-Host "Build successful!" -ForegroundColor Green

        # Show output location
        $outputDir = Join-Path $projectDir "bin\x64\$Configuration"
        $dllPath = Join-Path $outputDir "DL2HeadTracking.dll"
        if (Test-Path $dllPath) {
            $fileInfo = Get-Item $dllPath
            Write-Host ""
            Write-Host "Output: $dllPath" -ForegroundColor Gray
            Write-Host "Size:   $([math]::Round($fileInfo.Length / 1KB, 1)) KB" -ForegroundColor Gray
        }
        exit 0
    }

    # No Visual Studio found - show helpful error
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  ERROR: Visual Studio not found" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "This project requires Visual Studio with C++ tools." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Install Visual Studio:" -ForegroundColor Cyan
    Write-Host "  1. Download from: https://visualstudio.microsoft.com/downloads/" -ForegroundColor White
    Write-Host "  2. Choose 'Community' edition (free)" -ForegroundColor White
    Write-Host "  3. Select 'Desktop development with C++' workload" -ForegroundColor White
    Write-Host "  4. Complete installation" -ForegroundColor White
    Write-Host "  5. Re-run: pixi run deploy" -ForegroundColor Green
    Write-Host ""
    exit 1
}

# Check if solution file exists
if (-not (Test-Path $solutionFile)) {
    Write-Host "ERROR: Solution file not found: $solutionFile" -ForegroundColor Red
    exit 1
}

Write-Host "Using MSBuild: $($msbuild.Source)" -ForegroundColor Gray
Write-Host ""

# Run msbuild
$msbuildArgs = @(
    $solutionFile,
    "/p:Configuration=$Configuration",
    "/p:Platform=x64",
    "/m",
    "/nologo",
    "/v:minimal"
)

& msbuild @msbuildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  BUILD FAILED" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Check the errors above. Common issues:" -ForegroundColor Yellow
    Write-Host "  - Missing Windows SDK: Install via Visual Studio Installer" -ForegroundColor White
    Write-Host "  - Missing dependencies: Run 'pixi run setup'" -ForegroundColor White
    Write-Host "  - Syntax errors: Check the source file and line shown above" -ForegroundColor White
    Write-Host ""
    exit 1
}

Write-Host ""
Write-Host "Build successful!" -ForegroundColor Green
Write-Host ""

# Show output location
$outputDir = Join-Path $projectDir "bin\x64\$Configuration"
$dllPath = Join-Path $outputDir "DL2HeadTracking.dll"
if (Test-Path $dllPath) {
    $fileInfo = Get-Item $dllPath
    Write-Host "Output: $dllPath" -ForegroundColor Gray
    Write-Host "Size:   $([math]::Round($fileInfo.Length / 1KB, 1)) KB" -ForegroundColor Gray
}
