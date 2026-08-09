@echo off
setlocal EnableExtensions

rem Resolve Project from Project\Tools\Scripts without using a user-specific absolute path.
set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%~1"
set "CONFIGURATION=%~2"
set "NO_PAUSE=%~3"

if not defined PROJECT_DIR for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"
if not defined CONFIGURATION set "CONFIGURATION=Debug"

if not exist "%SCRIPT_DIR%BuildAssetCommon.ps1" (
    echo [RunBuildFonts] ERROR: BuildAssetCommon.ps1 not found.
    echo Expected: %SCRIPT_DIR%BuildAssetCommon.ps1
    set "EXIT_CODE=1"
    goto :finish
)

if not exist "%SCRIPT_DIR%BuildFonts.ps1" (
    echo [RunBuildFonts] ERROR: BuildFonts.ps1 not found.
    echo Do not leave names such as BuildFonts^(9^).ps1.
    echo Expected: %SCRIPT_DIR%BuildFonts.ps1
    set "EXIT_CODE=1"
    goto :finish
)

echo [RunBuildFonts] SCRIPT_DIR=%SCRIPT_DIR%
echo [RunBuildFonts] PROJECT_DIR=%PROJECT_DIR%
echo [RunBuildFonts] CONFIGURATION=%CONFIGURATION%

echo.
powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%SCRIPT_DIR%BuildFonts.ps1" -ProjectDir "%PROJECT_DIR%" -Configuration "%CONFIGURATION%"
set "EXIT_CODE=%ERRORLEVEL%"

:finish
echo.
if "%EXIT_CODE%"=="0" (
    echo [RunBuildFonts] SUCCESS
) else (
    echo [RunBuildFonts] FAILED. ExitCode=%EXIT_CODE%
)
if /I not "%NO_PAUSE%"=="--no-pause" pause
exit /b %EXIT_CODE%
