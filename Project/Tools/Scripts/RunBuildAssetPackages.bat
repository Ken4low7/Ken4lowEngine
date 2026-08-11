@echo off
setlocal EnableExtensions

rem Phase 8.4 packages cooked runtime outputs into deterministic chunk archives.
set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%~1"
set "CONFIGURATION=%~2"
set "NO_PAUSE=%~3"

if not defined PROJECT_DIR for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"
if not defined CONFIGURATION set "CONFIGURATION=Debug"

if not exist "%SCRIPT_DIR%BuildAssetPackages.py" (
    echo [RunBuildAssetPackages] ERROR: BuildAssetPackages.py not found.
    set "EXIT_CODE=1"
    goto :finish
)

echo [RunBuildAssetPackages] PROJECT_DIR=%PROJECT_DIR%
echo [RunBuildAssetPackages] CONFIGURATION=%CONFIGURATION%

python "%SCRIPT_DIR%BuildAssetPackages.py" --project-dir "%PROJECT_DIR%"
set "EXIT_CODE=%ERRORLEVEL%"

:finish
echo.
if "%EXIT_CODE%"=="0" (
    echo [RunBuildAssetPackages] SUCCESS
) else (
    echo [RunBuildAssetPackages] FAILED. ExitCode=%EXIT_CODE%
)
if /I not "%NO_PAUSE%"=="--no-pause" pause
exit /b %EXIT_CODE%
