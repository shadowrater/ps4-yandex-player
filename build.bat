@echo off
REM ═══════════════════════════════════════════════════════════════
REM  Yandex Music Player for PS4 - Windows Build Script
REM ═══════════════════════════════════════════════════════════════

echo ==========================================
echo   Yandex Music PS4 - Build Script
echo ==========================================
echo.

REM Check ORBIS_SDK
if "%ORBIS_SDK%"=="" (
    echo [ERROR] ORBIS_SDK not set!
    echo.
    echo Set it with:
    echo   set ORBIS_SDK=C:\path\to\openorbis-ps4-toolchain
    echo.
    echo Or add to System Environment Variables.
    pause
    exit /b 1
)

echo [INFO] ORBIS_SDK = %ORBIS_SDK%
echo.

REM Check compiler
if not exist "%ORBIS_SDK%\host_tools\bin\orbis-clang.exe" (
    echo [ERROR] orbis-clang not found in %ORBIS_SDK%\host_tools\bin\
    echo.
    echo Make sure OpenOrbis SDK is properly installed.
    pause
    exit /b 1
)

echo [INFO] Compiler found: orbis-clang
echo.

REM Build
echo [BUILD] Starting build...
make all

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build complete!
echo [OUTPUT] build\DEAD0001.pkg
echo.
echo Install on PS4:
echo   1. Copy .pkg to /data/pkg/ via FTP
echo   2. Install from Debug Settings > Package Installer
echo.
pause
