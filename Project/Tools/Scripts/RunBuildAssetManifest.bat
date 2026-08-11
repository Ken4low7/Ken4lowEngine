@echo off
setlocal

set "PROJECT_DIR=%~1"
if "%PROJECT_DIR%"=="" set "PROJECT_DIR=%CD%"

rem Phase 8 manifest generation is kept as a separate step so existing converters remain independently reusable.
python "%~dp0BuildAssetManifest.py" --project-dir "%PROJECT_DIR%" --fail-on-missing-output
exit /b %ERRORLEVEL%
