@echo off
REM ---------------------------------------------------------------------------
REM Build potplayer-vfi on Windows.
REM
REM Prerequisites:
REM   1. Visual Studio 2022 with "Desktop development with C++"
REM   2. Windows 10/11 SDK (strmif.h, d3d11.h)
REM   3. CMake 3.24+ on PATH
REM   4. DirectShow BaseClasses sources in third_party\baseclasses\*.cpp
REM      (microsoft/Windows-classic-samples -> Samples/Multimedia/DirectShow/
REM       BaseClasses)
REM ---------------------------------------------------------------------------
setlocal

where cmake >nul 2>nul || (
    echo [X] cmake not found in PATH
    exit /b 1
)

cmake -S "%~dp0." -B "%~dp0build" -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build "%~dp0build" --config Release || exit /b 1

echo.
echo [OK] Built: %~dp0build\Release\VfiFilter.ax
echo      Now run register.bat from an Administrator prompt.
