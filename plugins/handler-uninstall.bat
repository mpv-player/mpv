@echo OFF

:: Unattended install flag. When set, the script will not require user input.
set unattended=no
if "%1"=="/u" set unattended=yes

:: Make sure this is Windows Vista or later
call :ensure_vista

:: Make sure the script is running as admin
call :ensure_admin

:: Delete registry
call :del_verbs

echo Sussessful uninstall handler

:die
    if not [%1] == [] echo %~1
    if [%unattended%] == [yes] exit 1
    pause
    exit 1

:ensure_admin
    :: 'openfiles' is just a commmand that is present on all supported Windows
    :: versions, requires admin privileges and has no side effects, see:
    :: https://stackoverflow.com/questions/4051883/batch-script-how-to-check-for-admin-rights
    openfiles >nul 2>&1
    if errorlevel 1 (
        echo This batch script requires administrator privileges.
        echo Right-click on handler-uninstall.bat and select "Run as administrator".
        call :die
    )
    goto :EOF

:ensure_vista
    ver | find "XP" >nul
    if not errorlevel 1 (
        echo This batch script only works on Windows Vista and later. To create file
        echo associations on Windows XP, right click on a video file and use "Open with...".
        call :die
    )
    goto :EOF

:reg
    :: Wrap the reg command to check for errors
    >nul reg %*
    if errorlevel 1 set error=yes
    if [%error%] == [yes] echo Error in command: reg %*
    if [%error%] == [yes] call :die
    goto :EOF

:del_verbs
    call :reg delete "HKCR\mpv" /f
    goto :EOF
