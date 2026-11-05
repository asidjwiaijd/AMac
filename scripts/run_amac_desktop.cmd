@echo off
setlocal
set "desktop=%~dp0..\artifacts\desktop"
for %%F in (BasiliskII Q650.ROM Macintosh7.dsk) do (
    if not exist "%desktop%\%%F" (
        echo Missing %%F in "%desktop%".
        echo See the desktop preview section in AMAC_PORT.md.
        pause
        exit /b 1
    )
)
pushd "%desktop%" || exit /b 1
if not exist basilisk_ii_prefs copy "%~dp0..\examples\amac-desktop\basilisk_ii_prefs" basilisk_ii_prefs >nul
wsl.exe -d Ubuntu --cd "%CD%" -- flock -n -E 73 .session.lock ./BasiliskII --config ./basilisk_ii_prefs --nogui
set "result=%ERRORLEVEL%"
popd
if "%result%"=="73" echo The aMac desktop preview is already running.
if not "%result%"=="0" pause
exit /b %result%
