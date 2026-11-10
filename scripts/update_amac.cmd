@echo off
chcp 65001 >nul
cd /d "%~dp0.."
".pio-core\penv\Scripts\python.exe" -u tools\amac_update.py
pause
