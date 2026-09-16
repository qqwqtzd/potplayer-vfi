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

echo Unregistering "%AX%" ...
regsvr32 /s /u "%AX%"
echo [OK] Unregistered (ignore any error if it was not registered).
pause
exit /b 0
