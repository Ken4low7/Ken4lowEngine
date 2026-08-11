@echo off
setlocal EnableExtensions

rem Phase 8.5 force-cooks identical inputs twice and fails when any runtime byte changes.
set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%~1"
set "CONFIGURATION=%~2"
set "NO_PAUSE=%~3"

if not defined PROJECT_DIR for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"
if not defined CONFIGURATION set "CONFIGURATION=Debug"

if not exist "%SCRIPT_DIR%ValidateDeterministicCook.py" (
    echo [RunValidateDeterministicCook] ERROR: ValidateDeterministicCook.py not found.
    set "EXIT_CODE=1"
    goto :finish
)

echo [RunValidateDeterministicCook] PROJECT_DIR=%PROJECT_DIR%
echo [RunValidateDeterministicCook] CONFIGURATION=%CONFIGURATION%
echo [RunValidateDeterministicCook] NOTE: DDC is bypassed and all primary cookers run twice.

python "%SCRIPT_DIR%ValidateDeterministicCook.py" --project-dir "%PROJECT_DIR%" --configuration "%CONFIGURATION%"
set "EXIT_CODE=%ERRORLEVEL%"

:finish
echo.
if "%EXIT_CODE%"=="0" (
    echo [RunValidateDeterministicCook] DETERMINISTIC COOK PASSED
) else (
    echo [RunValidateDeterministicCook] FAILED. ExitCode=%EXIT_CODE%
)
if /I not "%NO_PAUSE%"=="--no-pause" pause
exit /b %EXIT_CODE%
