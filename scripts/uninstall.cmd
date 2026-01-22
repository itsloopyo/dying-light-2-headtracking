@echo off
:: ============================================
:: DL2 Head Tracking - Uninstall
:: ============================================
:: Based on cameraunlock-core/scripts/templates/uninstall.cmd
:: ============================================

:: --- CONFIG BLOCK ---
set "MOD_DISPLAY_NAME=DL2 Head Tracking"
set "GAME_EXE=DyingLightGame_x64_rwdi.exe"
set "GAME_DISPLAY_NAME=Dying Light 2"
set "STEAM_FOLDER_NAME=Dying Light 2"
set "ENV_VAR_NAME=DYING_LIGHT_2_PATH"
set "MOD_DLLS=DL2HeadTracking.asi HeadTracking.ini"
set "MOD_INTERNAL_NAME=DL2HeadTracking"
set "STATE_FILE=.headtracking-state.json"
set "LEGACY_DLLS=DL2HeadTracking.asi.bak"
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
echo === %MOD_DISPLAY_NAME% - Uninstall ===
echo.

set "GAME_PATH="
set "FORCE=0"

:: Parse arguments
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="/force" (
    set "FORCE=1"
    shift
    goto :parse_args
)
if /i "%~1"=="--force" (
    set "FORCE=1"
    shift
    goto :parse_args
)
:: Treat as game path — check for exe in subdirectory
if exist "%~1\ph\work\bin\x64\%GAME_EXE%" (
    set "GAME_PATH=%~1"
    shift
    goto :parse_args
)
echo ERROR: %GAME_EXE% not found at: %~1\ph\work\bin\x64\
echo.
exit /b 1

:args_done

:: --- Find game path ---
if not defined GAME_PATH (
    if defined %ENV_VAR_NAME% (
        call set "_ENV_PATH=%%%ENV_VAR_NAME%%%"
        if exist "!_ENV_PATH!\ph\work\bin\x64\%GAME_EXE%" (
            set "GAME_PATH=!_ENV_PATH!"
        )
    )
)

if not defined GAME_PATH call :find_steam_game
if not defined GAME_PATH call :find_gog_game
if not defined GAME_PATH call :find_epic_game
if not defined GAME_PATH call :find_game_in_dirs

if not defined GAME_PATH (
    echo ERROR: Could not find %GAME_DISPLAY_NAME% installation.
    echo.
    exit /b 1
)

echo Game found: %GAME_PATH%
echo.

set "EXE_DIR=%GAME_PATH%\ph\work\bin\x64"

:: --- Check if game is running ---
tasklist /fi "imagename eq %GAME_EXE%" 2>nul | findstr /i "%GAME_EXE%" >nul 2>&1
if not errorlevel 1 (
    echo ERROR: %GAME_DISPLAY_NAME% is currently running.
    echo Please close the game before uninstalling.
    echo.
    exit /b 1
)

:: --- Remove mod files ---
echo Removing mod files...

set "PLUGINS_PATH=%EXE_DIR%"
set "REMOVED=0"

for %%f in (%MOD_DLLS%) do (
    if exist "%PLUGINS_PATH%\%%f" (
        del "%PLUGINS_PATH%\%%f"
        echo   Removed: %%f
        set /a REMOVED+=1
    )
)

:: Remove legacy files from previous versions
if defined LEGACY_DLLS (
    for %%f in (%LEGACY_DLLS%) do (
        if exist "%PLUGINS_PATH%\%%f" (
            del "%PLUGINS_PATH%\%%f"
            echo   Removed: %%f ^(legacy^)
            set /a REMOVED+=1
        )
    )
)

if "!REMOVED!"=="0" echo   No mod files found

:: --- Check if we should remove ASI loader ---
if "!FORCE!"=="1" (
    echo.
    echo Removing ASI loader ^(--force^)...
    goto :remove_modloader
)

set "WE_INSTALLED=0"
if exist "%GAME_PATH%\%STATE_FILE%" (
    findstr /c:"installed_by_us" "%GAME_PATH%\%STATE_FILE%" 2>nul | findstr /c:"true" >nul 2>&1
    if not errorlevel 1 set "WE_INSTALLED=1"
)

if "!WE_INSTALLED!"=="0" (
    echo.
    echo ASI loader was not installed by this mod - leaving intact. Use --force to remove anyway.
    goto :cleanup_state
)

echo.
echo Removing ASI loader ^(it was installed by this mod^)...

:remove_modloader

if exist "%EXE_DIR%\winmm.dll" (
    del "%EXE_DIR%\winmm.dll"
    echo   Removed: winmm.dll
)

:cleanup_state
if exist "%GAME_PATH%\%STATE_FILE%" (
    del "%GAME_PATH%\%STATE_FILE%"
    echo   Removed: state file
)

echo.
echo === Uninstall Complete ===
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
