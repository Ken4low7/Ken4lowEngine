@echo off
setlocal EnableExtensions

rem Run Font before Texture because font atlas PNG files are texture source assets.
set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%~1"
set "CONFIGURATION=%~2"

if not defined PROJECT_DIR for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"
if not defined CONFIGURATION set "CONFIGURATION=Debug"

for %%F in (BuildAssetCommon.ps1 BuildFonts.ps1 BuildTextures.ps1 BuildMeshes.ps1) do (
    if not exist "%SCRIPT_DIR%%%F" (
        echo [RunBuildAssets] ERROR: %%F not found.
        echo Expected: %SCRIPT_DIR%%%F
        echo Remove suffixes such as ^(9^) or ^(4^) from the file name.
        set "EXIT_CODE=1"
        goto :finish
    )
)

echo [RunBuildAssets] PROJECT_DIR=%PROJECT_DIR%
echo [RunBuildAssets] CONFIGURATION=%CONFIGURATION%
echo.

call "%SCRIPT_DIR%RunBuildFonts.bat" "%PROJECT_DIR%" "%CONFIGURATION%" --no-pause
if errorlevel 1 (
    set "EXIT_CODE=%ERRORLEVEL%"
    echo [RunBuildAssets] Stopped at Font build.
    goto :finish
)

call "%SCRIPT_DIR%RunBuildTextures.bat" "%PROJECT_DIR%" "%CONFIGURATION%" --no-pause
if errorlevel 1 (
    set "EXIT_CODE=%ERRORLEVEL%"
    echo [RunBuildAssets] Stopped at Texture build.
    goto :finish
)

call "%SCRIPT_DIR%RunBuildMeshes.bat" "%PROJECT_DIR%" "%CONFIGURATION%" --no-pause
if errorlevel 1 (
    set "EXIT_CODE=%ERRORLEVEL%"
    echo [RunBuildAssets] Stopped at Mesh build.
    goto :finish
)

set "EXIT_CODE=0"

:finish
echo.
if "%EXIT_CODE%"=="0" (
    echo [RunBuildAssets] ALL BUILDS SUCCEEDED
) else (
    echo [RunBuildAssets] FAILED. ExitCode=%EXIT_CODE%
)
pause
exit /b %EXIT_CODE%
