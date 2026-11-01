@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"
set "SWORDIGO_DATA_DIR=%~dp0"
set "LOGDIR=%LOCALAPPDATA%\SwordigoDesktop\logs"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
set "LOG=%LOGDIR%\latest.log"
swordfare.exe --check-data --lib engine/v1.4.13/arm64-v8a/libswordigo.so --assets assets > "%LOG%" 2>&1
if errorlevel 1 (
    type "%LOG%"
    echo.
    echo Setup incomplete. Read WINDOWS_CLIENT.md. Log: "%LOG%"
    pause
    exit /b 2
)
swordfare.exe --no-launcher --dynarmic --lib engine/v1.4.13/arm64-v8a/libswordigo.so --assets assets %* >> "%LOG%" 2>&1
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
    echo Swordigo exited with code %RESULT%. Log: "%LOG%"
    pause
)
exit /b %RESULT%
