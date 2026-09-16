@echo off
setlocal
cd /d "%~dp0"

REM Self-elevate to Administrator.
net session >nul 2>&1
if not "%errorlevel%"=="0" (
    echo Requesting administrator privileges...
    powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
    exit /b
)

set "AX=%~dp0VfiFilter.ax"
if not exist "%AX%" (
    echo [X] VfiFilter.ax was not found next to this script.
    pause
    exit /b 1
)

if not exist "%~dp0shaders\vfi.hlsl" (
    echo [!] Warning: shaders\vfi.hlsl not found. Interpolation needs it next to the filter.
)

echo Registering "%AX%" ...
regsvr32 /s "%AX%"
if errorlevel 1 (
    echo [X] Registration failed.
    pause
    exit /b 1
)

echo.
echo [OK] Filter registered.
echo.
echo Next, in PotPlayer:
echo    Options - Filter - Add external filter - select VfiFilter.ax
echo    then set it to "Always use" and open its Properties to enable
echo    frame interpolation.
echo.
echo Requires an NVIDIA RTX GPU and nvofapi64.dll (installed with the driver).
pause
exit /b 0
