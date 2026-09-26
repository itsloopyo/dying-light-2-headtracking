#!/usr/bin/env pwsh
#Requires -Version 5.1
# Custom packaging for DL2 Head Tracking (C++ project, no .csproj)
# Produces two ZIPs:
#   - DL2HeadTracking-v{version}-installer.zip (GitHub Release: install.cmd + plugins/ + docs)
#   - DL2HeadTracking-v{version}-nexus.zip   (Nexus Mods: extract-to-game-folder layout)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = 'SilentlyContinue'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectDir "cameraunlock-core\powershell\ReleaseWorkflow.psm1") -Force

# Get version from manifest
$manifest = Get-Content (Join-Path $projectDir "manifest.json") | ConvertFrom-Json
$version = $manifest.version

Write-Host "=== DL2 Head Tracking - Package Release ===" -ForegroundColor Magenta
Write-Host ""
Write-Host "Version: $version" -ForegroundColor Cyan
Write-Host ""

$releaseDir = Join-Path $projectDir "release"

# Create release directory
if (-not (Test-Path $releaseDir)) {
    New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
}

# Validate required source files upfront
$asiPath = Join-Path $projectDir "bin/Release/DL2HeadTracking.asi"
if (-not (Test-Path $asiPath)) {
    throw "DL2HeadTracking.asi not found at: $asiPath"
}

$vendorAsiDir = Join-Path $projectDir "vendor/ultimate-asi-loader"
$vendorAsiDll = Join-Path $vendorAsiDir "dinput8.dll"
if (-not (Test-Path $vendorAsiDll)) {
    throw "Bundled ASI loader missing: $vendorAsiDll"
}

$launcherManifestPath = Join-Path $projectDir "launcher-manifest.json"
if (-not (Test-Path $launcherManifestPath)) {
    throw "launcher-manifest.json not found at: $launcherManifestPath"
}

$scriptsDir = Join-Path $projectDir "scripts"
foreach ($script in @("install.cmd", "uninstall.cmd")) {
    $scriptPath = Join-Path $scriptsDir $script
    if (-not (Test-Path $scriptPath)) {
        throw "Required script not found: $scriptPath"
    }
}

# --- GitHub Release ZIP (with installer) ---

Write-Host "--- GitHub Release ZIP ---" -ForegroundColor Yellow
Write-Host ""

$ghStagingDir = Join-Path $releaseDir "staging-github"
if (Test-Path $ghStagingDir) { Remove-Item -Recurse -Force $ghStagingDir }
New-Item -ItemType Directory -Path $ghStagingDir -Force | Out-Null

# Copy install/uninstall scripts
foreach ($script in @("install.cmd", "uninstall.cmd")) {
    Copy-Item (Join-Path $scriptsDir $script) -Destination $ghStagingDir -Force
    Write-Host "  $script" -ForegroundColor Green
}

# The only launcher manifest. Stamp the real release version into
# mod_info.version and drop it at the installer ZIP root; the launcher deploys
# the package from files/loader/runtime_requirements/dependencies.
$manifestJson = Get-Content $launcherManifestPath -Raw | ConvertFrom-Json
$manifestJson.mod_info.version = $version
# Set-Content -Encoding UTF8 on Windows PowerShell 5.1 writes a BOM, which
# serde_json rejects. Write through the .NET API with a no-BOM encoder.
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText(
    (Join-Path $ghStagingDir "launcher-manifest.json"),
    ($manifestJson | ConvertTo-Json -Depth 10),
    $utf8NoBom
)
Write-Host "  launcher-manifest.json (v$version)" -ForegroundColor Green

# Copy mod files to plugins subfolder
$pluginsDir = Join-Path $ghStagingDir "plugins"
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null

Copy-Item $asiPath -Destination $pluginsDir -Force
Write-Host "  plugins/DL2HeadTracking.asi" -ForegroundColor Green

# Bundle Ultimate ASI Loader (MIT, see THIRD-PARTY-NOTICES.md) so install.cmd
# has no GitHub dependency at install time.
$ghVendorDir = Join-Path $ghStagingDir "vendor/ultimate-asi-loader"
New-Item -ItemType Directory -Path $ghVendorDir -Force | Out-Null
# The upstream MIT licence has to travel with the loader binary, so a missing
# LICENSE is a compliance failure - throw rather than skipping the copy.
foreach ($vendorFile in @("dinput8.dll", "LICENSE", "README.md")) {
    $src = Join-Path $vendorAsiDir $vendorFile
    if (-not (Test-Path $src)) {
        throw "Vendored ASI loader file not found: $src"
    }
    Copy-Item $src -Destination $ghVendorDir -Force
    Write-Host "  vendor/ultimate-asi-loader/$vendorFile" -ForegroundColor Green
}

# Copy documentation. LICENSE and THIRD-PARTY-NOTICES.md carry the MIT/BSD
# notices for everything linked into the .asi and for the bundled ASI loader,
# so a missing one is a licence violation, not a cosmetic gap - fail the build.
$docFiles = @("README.md", "LICENSE", "CHANGELOG.md", "THIRD-PARTY-NOTICES.md")
foreach ($doc in $docFiles) {
    $docPath = Join-Path $projectDir $doc
    if (-not (Test-Path $docPath)) {
        throw "Required document not found: $docPath"
    }
    Copy-Item $docPath -Destination $ghStagingDir -Force
    Write-Host "  $doc" -ForegroundColor Green
}

Copy-SharedBundle -StagingDir $ghStagingDir

$ghZipName = "DL2HeadTracking-v$version-installer.zip"
$ghZipPath = Join-Path $releaseDir $ghZipName
if (Test-Path $ghZipPath) { Remove-Item $ghZipPath -Force }

Write-Host ""
Write-Host "Creating GitHub ZIP..." -ForegroundColor Cyan

Push-Location $ghStagingDir
try {
    Compress-Archive -Path ".\*" -DestinationPath $ghZipPath -Force
} finally {
    Pop-Location
}
Remove-Item -Recurse -Force $ghStagingDir

$ghZipSize = (Get-Item $ghZipPath).Length / 1KB
Write-Host ("  $ghZipPath ({0:N1} KB)" -f $ghZipSize) -ForegroundColor Green

# --- Nexus Mods ZIP (extract-to-game-folder) ---

Write-Host ""
Write-Host "--- Nexus Mods ZIP ---" -ForegroundColor Yellow
Write-Host ""

$nexusStagingDir = Join-Path $releaseDir "staging-nexus"
if (Test-Path $nexusStagingDir) { Remove-Item -Recurse -Force $nexusStagingDir }

# Mirror game directory structure: ph/work/bin/x64/
$nexusGameDir = Join-Path $nexusStagingDir "ph\work\bin\x64"
New-Item -ItemType Directory -Path $nexusGameDir -Force | Out-Null

Copy-Item $asiPath -Destination $nexusGameDir -Force
Write-Host "  ph/work/bin/x64/DL2HeadTracking.asi" -ForegroundColor Green

# No Ultimate ASI Loader here. Vendoring the loader is for our own installer
# and for Lopari; a Nexus upload must not redistribute another author's tool,
# so Nexus users install the loader themselves (README, Manual Installation).

# The Nexus ZIP is still a binary distribution: the .asi statically links
# MinHook, ImGui, Kiero, inih and cameraunlock-core. MIT and BSD both require
# the notices to accompany the binary, so they ship at the ZIP root.
foreach ($doc in @("LICENSE", "THIRD-PARTY-NOTICES.md", "README.md")) {
    $docPath = Join-Path $projectDir $doc
    if (-not (Test-Path $docPath)) {
        throw "Required document not found: $docPath"
    }
    Copy-Item $docPath -Destination $nexusStagingDir -Force
    Write-Host "  $doc" -ForegroundColor Green
}

$nexusZipName = "DL2HeadTracking-v$version-nexus.zip"
$nexusZipPath = Join-Path $releaseDir $nexusZipName
if (Test-Path $nexusZipPath) { Remove-Item $nexusZipPath -Force }

Write-Host ""
Write-Host "Creating Nexus ZIP..." -ForegroundColor Cyan

Push-Location $nexusStagingDir
try {
    Compress-Archive -Path ".\*" -DestinationPath $nexusZipPath -Force
} finally {
    Pop-Location
}
Remove-Item -Recurse -Force $nexusStagingDir

$nexusZipSize = (Get-Item $nexusZipPath).Length / 1KB
Write-Host ("  $nexusZipPath ({0:N1} KB)" -f $nexusZipSize) -ForegroundColor Green

# --- Licence compliance check on the finished ZIPs ---

Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($zipPath in @($ghZipPath, $nexusZipPath)) {
    $zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
    try {
        $entries = $zip.Entries.FullName
    } finally {
        $zip.Dispose()
    }
    foreach ($required in @("LICENSE", "THIRD-PARTY-NOTICES.md")) {
        if ($entries -notcontains $required) {
            throw "$(Split-Path -Leaf $zipPath) ships binaries without $required at its root - that is a licence violation, not a packaging nit."
        }
    }
}

# The Nexus ZIP must carry our own payload only. Redistributing Ultimate ASI
# Loader is fine in our installer and in Lopari, where we control the flow, but
# a Nexus upload must not bundle another author's tool - it goes in the
# requirements list instead.
$nexusZip = [System.IO.Compression.ZipFile]::OpenRead($nexusZipPath)
try {
    $nexusEntries = $nexusZip.Entries.FullName
} finally {
    $nexusZip.Dispose()
}
foreach ($forbidden in @("winmm.dll", "dinput8.dll", "version.dll")) {
    $hit = $nexusEntries | Where-Object { (Split-Path -Leaf $_) -eq $forbidden }
    if ($hit) {
        throw "$nexusZipName bundles the ASI loader ($hit). The Nexus ZIP ships our files only; the loader is a stated requirement there."
    }
}
Write-Host ""
Write-Host "Licence notices present in both ZIPs" -ForegroundColor Green

# --- Summary ---

Write-Host ""
Write-Host "=== Package Complete ===" -ForegroundColor Magenta
Write-Host ""
Write-Host ("GitHub Release: $ghZipPath ({0:N1} KB)" -f $ghZipSize) -ForegroundColor Green
Write-Host ("Nexus Mods:     $nexusZipPath ({0:N1} KB)" -f $nexusZipSize) -ForegroundColor Green

# Output both zip paths for CI capture (one per line)
Write-Output $ghZipPath
Write-Output $nexusZipPath
