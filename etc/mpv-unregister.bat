@echo off
setlocal

"%~dp0/mpv" --unregister
if %errorlevel% neq 0 (
    echo Deregistration failed. Make sure mpv is in the same folder as this script.
    pause
    exit /b %errorlevel%
)

pause
endlocal
