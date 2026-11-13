@echo off
chcp 65001 >nul
cd /d "%~dp0.."
if exist "release\desktop\aMac-Control.exe" (
    start "" "release\desktop\aMac-Control.exe"
    exit /b 0
)
if exist ".pio-core\penv\Scripts\pythonw.exe" (
    start "" ".pio-core\penv\Scripts\pythonw.exe" "tools\amac_control_app.py"
    exit /b 0
)
echo 请先取得已打包的 aMac-Control.exe，或在 Python 环境安装 tools\requirements.txt。
echo 源码运行命令：python tools\amac_control_app.py
pause
