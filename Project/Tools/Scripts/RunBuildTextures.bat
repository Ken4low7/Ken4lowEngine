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
    echo [RunBuildTextures] ERROR: BuildAssetCommon.ps1 not found.
    echo Expected: %SCRIPT_DIR%BuildAssetCommon.ps1
    set "EXIT_CODE=1"
    goto :finish
)

if not exist "%SCRIPT_DIR%BuildTextures.ps1" (
    echo [RunBuildTextures] ERROR: BuildTextures.ps1 not found.
    echo Do not leave names such as BuildTextures^(4^).ps1.
    echo Expected: %SCRIPT_DIR%BuildTextures.ps1
    set "EXIT_CODE=1"
    goto :finish
)

echo [RunBuildTextures] SCRIPT_DIR=%SCRIPT_DIR%
echo [RunBuildTextures] PROJECT_DIR=%PROJECT_DIR%
echo [RunBuildTextures] CONFIGURATION=%CONFIGURATION%

echo.
powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%SCRIPT_DIR%BuildTextures.ps1" -ProjectDir "%PROJECT_DIR%" -Configuration "%CONFIGURATION%"
set "EXIT_CODE=%ERRORLEVEL%"

:finish
echo.
if "%EXIT_CODE%"=="0" (
    echo [RunBuildTextures] SUCCESS
) else (
    echo [RunBuildTextures] FAILED. ExitCode=%EXIT_CODE%
)
if /I not "%NO_PAUSE%"=="--no-pause" pause
exit /b %EXIT_CODE%
