@echo OFF

:: Unattended install flag. When set, the script will not require user input.
set unattended=no
if "%1"=="/u" set unattended=yes

:: Make sure this is Windows Vista or later
call :ensure_vista

:: Make sure the script is running as admin
call :ensure_admin

:: Get mpv.exe location
call :check_binary

:: Add registry
call :add_verbs

echo Sussessful install handler
echo Enjoy!

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
        echo Right-click on handler-install.bat and select "Run as administrator".
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

:check_binary
    cd /D %~dp0
    set mpv_handler_path=%cd%\mpv-handler.exe
    set mpv_handler_conf=%cd%\config.toml
    if not exist "%mpv_handler_path%" call :die "Not found mpv-handler.exe"
    if not exist "%mpv_handler_conf%" call :die "Not found config.toml"
    goto :EOF

:reg
    :: Wrap the reg command to check for errors
    >nul reg %*
    if errorlevel 1 set error=yes
    if [%error%] == [yes] echo Error in command: reg %*
    if [%error%] == [yes] call :die
    goto :EOF

:add_verbs
    call :reg add "HKCR\mpv" /d "URL:mpv Handler link" /f
    call :reg add "HKCR\mpv" /v "Content Type" /d "application/x-mpv" /f
    call :reg add "HKCR\mpv" /v "URL Protocol" /f
    call :reg add "HKCR\mpv\DefaultIcon" /d "\"%mpv_exe_path%\",1" /f
    call :reg add "HKCR\mpv\shell\open\command" /d "\"%mpv_handler_path%\" \"%%%%1\"" /f
    goto :EOF
