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
    echo [X] Registration failed - showing details...
    regsvr32 "%AX%"
    echo.
    echo Common causes:
    echo   * The file was downloaded and is blocked by Windows:
    echo     right-click VfiFilter.ax - Properties - tick "Unblock" - OK,
    echo     then run this script again.
    echo   * A 32-bit regsvr32 was used for a 64-bit .ax:
    echo     run from the normal 64-bit command prompt.
    echo   * Missing Microsoft Visual C++ Redistributable (VC++ 2015-2022 x64).
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
