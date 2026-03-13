@echo off
:: ============================================
:: DL2 Head Tracking - Install
:: ============================================
:: Based on cameraunlock-core/scripts/templates/install.cmd
:: NOTE: Uses ASI Loader, exe is in ph\work\bin\x64\ subdirectory
:: ============================================

:: --- CONFIG BLOCK ---
set "MOD_DISPLAY_NAME=DL2 Head Tracking"
set "GAME_EXE=DyingLightGame_x64_rwdi.exe"
set "GAME_DISPLAY_NAME=Dying Light 2"
set "STEAM_FOLDER_NAME=Dying Light 2"
set "ENV_VAR_NAME=DYING_LIGHT_2_PATH"
set "MOD_FILES=DL2HeadTracking.asi HeadTracking.ini"
set "MOD_INTERNAL_NAME=DL2HeadTracking"
set "MOD_VERSION=1.0.3"
set "STATE_FILE=.headtracking-state.json"
set "MOD_CONTROLS=Controls:&echo   Home - Recenter head tracking&echo   End  - Toggle head tracking on/off"
set "GOG_IDS="
set "SEARCH_DIRS="
:: --- END CONFIG BLOCK ---

call :main %*
set "_EC=%errorlevel%"
echo.
pause
exit /b %_EC%

:main
setlocal enabledelayedexpansion

echo.
echo === %MOD_DISPLAY_NAME% - Install ===
echo.

set "SCRIPT_DIR=%~dp0"
set "GAME_PATH="

:: --- Find game path ---

:: Check command line argument
if not "%~1"=="" (
    if exist "%~1\ph\work\bin\x64\%GAME_EXE%" (
        set "GAME_PATH=%~1"
        goto :found_game
    )
    echo ERROR: %GAME_EXE% not found at: %~1\ph\work\bin\x64\
    echo.
    exit /b 1
)

:: Check environment variable
if defined %ENV_VAR_NAME% (
    call set "_ENV_PATH=%%%ENV_VAR_NAME%%%"
    if exist "!_ENV_PATH!\ph\work\bin\x64\%GAME_EXE%" (
        set "GAME_PATH=!_ENV_PATH!"
        goto :found_game
    )
)

:: Check Steam (try multiple folder name variants)
call :find_steam_game
if defined GAME_PATH goto :found_game

set "STEAM_FOLDER_NAME=Dying Light 2 Stay Human"
call :find_steam_game
if defined GAME_PATH goto :found_game

set "STEAM_FOLDER_NAME=Dying Light 2 Reloaded Edition"
call :find_steam_game
if defined GAME_PATH goto :found_game

:: Reset for display
set "STEAM_FOLDER_NAME=Dying Light 2"

:: Check GOG
call :find_gog_game
if defined GAME_PATH goto :found_game

:: Check Epic
call :find_epic_game
if defined GAME_PATH goto :found_game

:: Check common directories
call :find_game_in_dirs
if defined GAME_PATH goto :found_game

echo ERROR: Could not find %GAME_DISPLAY_NAME% installation.
echo.
echo Please either:
echo   1. Set %ENV_VAR_NAME% environment variable to your game folder
echo   2. Run: install.cmd "C:\path\to\game"
echo.
exit /b 1

:found_game
echo Game found: %GAME_PATH%
echo.

set "EXE_DIR=%GAME_PATH%\ph\work\bin\x64"

:: --- Check if game is running ---
tasklist /fi "imagename eq %GAME_EXE%" 2>nul | findstr /i "%GAME_EXE%" >nul 2>&1
if not errorlevel 1 (
    echo ERROR: %GAME_DISPLAY_NAME% is currently running.
    echo Please close the game before installing.
    echo.
    exit /b 1
)

:: --- Check ASI Loader ---
if not exist "%EXE_DIR%\winmm.dll" (
    echo ASI Loader not found. Installing...
    echo.
    call :install_asi_loader
    if errorlevel 1 exit /b 1
) else (
    echo ASI Loader found.
)
echo.

:: --- Deploy mod files ---
echo Deploying mod files...

set "DEPLOY_DIR=%EXE_DIR%"
set "FILES_DIR=%SCRIPT_DIR%plugins"

set "DEPLOY_FAILED=0"
for %%f in (%MOD_FILES%) do (
    if exist "%FILES_DIR%\%%f" (
        copy /y "%FILES_DIR%\%%f" "%DEPLOY_DIR%\" >nul
        echo   Deployed %%f
    ) else (
        echo   ERROR: %%f not found in plugins folder
        set "DEPLOY_FAILED=1"
    )
)

if "!DEPLOY_FAILED!"=="1" (
    echo.
    echo ========================================
    echo   Deployment Failed!
    echo ========================================
    echo.
    exit /b 1
)

:: --- Update state file ---
:: Preserve installed_by_us flag from previous state
set "WE_INSTALLED=false"
if exist "%GAME_PATH%\%STATE_FILE%" (
    findstr /c:"installed_by_us" "%GAME_PATH%\%STATE_FILE%" 2>nul | findstr /c:"true" >nul 2>&1
    if not errorlevel 1 set "WE_INSTALLED=true"
)

> "%GAME_PATH%\%STATE_FILE%" (
    echo {
    echo   "framework": {
    echo     "type": "ASILoader",
    echo     "installed_by_us": !WE_INSTALLED!
    echo   },
    echo   "mod": {
    echo     "name": "%MOD_INTERNAL_NAME%",
    echo     "version": "%MOD_VERSION%"
    echo   }
    echo }
)

echo.
echo ========================================
echo   Deployment Complete!
echo ========================================
echo.
echo %MOD_DISPLAY_NAME% has been deployed to:
echo   %DEPLOY_DIR%
echo.
echo Start the game to use the mod!
if defined MOD_CONTROLS (
    echo.
    echo %MOD_CONTROLS%
)
echo.
exit /b 0

:: ============================================
:: Find game in Steam libraries
:: ============================================
:find_steam_game
set "STEAM_PATH="

:: Get Steam install path from registry (64-bit)
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\Valve\Steam" /v InstallPath 2^>nul') do set "STEAM_PATH=%%b"

:: Try 32-bit registry
if not defined STEAM_PATH (
    for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Valve\Steam" /v InstallPath 2^>nul') do set "STEAM_PATH=%%b"
)

:: Check default Steam library
if defined STEAM_PATH (
    if exist "%STEAM_PATH%\steamapps\common\%STEAM_FOLDER_NAME%\ph\work\bin\x64\%GAME_EXE%" (
        set "GAME_PATH=%STEAM_PATH%\steamapps\common\%STEAM_FOLDER_NAME%"
        exit /b 0
    )
)

:: Parse libraryfolders.vdf for additional Steam library paths
if defined STEAM_PATH (
    set "VDF_FILE=%STEAM_PATH%\steamapps\libraryfolders.vdf"
    if exist "!VDF_FILE!" (
        for /f "tokens=1,2 delims=	 " %%a in ('findstr /c:"\"path\"" "!VDF_FILE!" 2^>nul') do (
            set "_LIB_PATH=%%~b"
            set "_LIB_PATH=!_LIB_PATH:\\=\!"
            if exist "!_LIB_PATH!\steamapps\common\%STEAM_FOLDER_NAME%\ph\work\bin\x64\%GAME_EXE%" (
                set "GAME_PATH=!_LIB_PATH!\steamapps\common\%STEAM_FOLDER_NAME%"
                exit /b 0
            )
        )
    )
)

exit /b 1

:: ============================================
:: Find game in GOG registry
:: ============================================
:find_gog_game
if not defined GOG_IDS exit /b 1
for %%g in (%GOG_IDS%) do (
    for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\%%g" /v path 2^>nul') do (
        if exist "%%b\ph\work\bin\x64\%GAME_EXE%" ( set "GAME_PATH=%%b" & exit /b 0 )
    )
    for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\GOG.com\Games\%%g" /v path 2^>nul') do (
        if exist "%%b\ph\work\bin\x64\%GAME_EXE%" ( set "GAME_PATH=%%b" & exit /b 0 )
    )
)
exit /b 1

:: ============================================
:: Find game in Epic Games manifests
:: ============================================
:find_epic_game
set "_EPIC_MANIFESTS=%ProgramData%\Epic\EpicGamesLauncher\Data\Manifests"
if not exist "%_EPIC_MANIFESTS%" exit /b 1
for %%m in ("%_EPIC_MANIFESTS%\*.item") do (
    for /f "usebackq delims=" %%l in ("%%m") do (
        set "_EL=%%l"
        if not "!_EL:InstallLocation=!"=="!_EL!" (
            set "_EL=!_EL:*InstallLocation=!"
            set "_EL=!_EL:~4!"
            set "_EL=!_EL:~0,-2!"
            set "_EL=!_EL:\\=\!"
            if exist "!_EL!\ph\work\bin\x64\%GAME_EXE%" ( set "GAME_PATH=!_EL!" & exit /b 0 )
        )
    )
)
exit /b 1

:: ============================================
:: Find game by scanning common directories
:: ============================================
:find_game_in_dirs
if not defined SEARCH_DIRS exit /b 1
for %%d in (%SEARCH_DIRS%) do (
    if exist "%%~d\ph\work\bin\x64\%GAME_EXE%" ( set "GAME_PATH=%%~d" & exit /b 0 )
    for /f "delims=" %%p in ('dir /b /ad "%%~d" 2^>nul') do (
        if exist "%%~d\%%p\ph\work\bin\x64\%GAME_EXE%" ( set "GAME_PATH=%%~d\%%p" & exit /b 0 )
        for /f "delims=" %%s in ('dir /b /ad "%%~d\%%p" 2^>nul') do (
            if exist "%%~d\%%p\%%s\ph\work\bin\x64\%GAME_EXE%" ( set "GAME_PATH=%%~d\%%p\%%s" & exit /b 0 )
        )
    )
)
exit /b 1

:: ============================================
:: Install Ultimate ASI Loader
:: ============================================
:install_asi_loader
set "ASI_URL=https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/Ultimate-ASI-Loader_x64.zip"
set "ASI_ZIP=%TEMP%\ASILoader_install.zip"

echo   Downloading Ultimate ASI Loader...
curl -fL -o "%ASI_ZIP%" "%ASI_URL%"
if errorlevel 1 (
    echo   ERROR: Download failed. Check your internet connection.
    exit /b 1
)

echo   Extracting to game directory...
tar -xf "%ASI_ZIP%" -C "%EXE_DIR%"
if errorlevel 1 (
    echo   ERROR: Extraction failed.
    del "%ASI_ZIP%" 2>nul
    exit /b 1
)
del "%ASI_ZIP%" 2>nul

:: Rename dinput8.dll to winmm.dll (DL2 compatibility)
if exist "%EXE_DIR%\dinput8.dll" (
    move /y "%EXE_DIR%\dinput8.dll" "%EXE_DIR%\winmm.dll" >nul
)

:: Write state file marking that we installed ASI loader
> "%GAME_PATH%\%STATE_FILE%" (
    echo {
    echo   "framework": {
    echo     "type": "ASILoader",
    echo     "installed_by_us": true
    echo   }
    echo }
)

echo   Ultimate ASI Loader installed successfully!
exit /b 0
