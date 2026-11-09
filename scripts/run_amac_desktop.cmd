@echo off
setlocal
set "desktop=%~dp0..\artifacts\desktop"
for %%F in (BasiliskII-birthday Q650.ROM Macintosh7.dsk) do (
    if not exist "%desktop%\%%F" (
        echo Missing %%F in "%desktop%".
        echo See the desktop preview section in AMAC_PORT.md.
        pause
        exit /b 1
    )
)
powershell.exe -NoProfile -File "%~dp0start_amac_desktop.ps1"
exit /b %ERRORLEVEL%
