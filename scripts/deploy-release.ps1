#!/usr/bin/env pwsh
# Deploy release build to game directory

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
& "$scriptDir\deploy.ps1" "Release"
