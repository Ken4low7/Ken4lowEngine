@echo off
setlocal EnableExtensions

rem Phase 8.3 resolves dependency changes before choosing which cooker categories need to run.
set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%~1"
set "CONFIGURATION=%~2"
set "NO_PAUSE=%~3"

if not defined PROJECT_DIR for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"
if not defined CONFIGURATION set "CONFIGURATION=Debug"

if not exist "%SCRIPT_DIR%BuildIncrementalAssets.py" (
    echo [RunBuildIncrementalAssets] ERROR: BuildIncrementalAssets.py not found.
    set "EXIT_CODE=1"
    goto :finish
)

echo [RunBuildIncrementalAssets] PROJECT_DIR=%PROJECT_DIR%
echo [RunBuildIncrementalAssets] CONFIGURATION=%CONFIGURATION%

python "%SCRIPT_DIR%BuildIncrementalAssets.py" --project-dir "%PROJECT_DIR%" --configuration "%CONFIGURATION%" --execute
set "EXIT_CODE=%ERRORLEVEL%"

:finish
echo.
if "%EXIT_CODE%"=="0" (
    echo [RunBuildIncrementalAssets] SUCCESS
) else (
    echo [RunBuildIncrementalAssets] FAILED. ExitCode=%EXIT_CODE%
)
if /I not "%NO_PAUSE%"=="--no-pause" pause
exit /b %EXIT_CODE%
