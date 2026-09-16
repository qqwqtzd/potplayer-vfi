@echo off
REM Register the filter so PotPlayer/DirectShow can see it.
REM Must be run from an Administrator command prompt (writes HKCR).
setlocal

set "AX=%~dp0build\Release\VfiFilter.ax"
if not exist "%AX%" (
    echo [X] %AX% not found - run build.bat first.
    exit /b 1
)

echo Registering "%AX%" ...
regsvr32 /s "%AX%"
if errorlevel 1 (
    echo [X] regsvr32 failed. Are you running as Administrator?
    exit /b 1
)
echo [OK] Registered. Add "PotPlayer VFI (frame interpolation)" in
echo      PotPlayer - Preferences - Filter - Add external filter.
