param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $ProjectRoot
try {
    $Icon = Join-Path $ProjectRoot 'assets/happymacIcon.png'
    & $Python -m PyInstaller --version
    if ($LASTEXITCODE -ne 0) { throw '请先安装 PyInstaller 和 pyserial，并确认 Python 包含 tkinter。' }
    & $Python -m PyInstaller --noconfirm --clean --onefile --windowed `
        --name aMac-Control --paths tools --hidden-import serial.tools.list_ports `
        --icon $Icon --add-data ($Icon + ';assets') `
        --distpath release/desktop --workpath artifacts/desktop-control/build `
        --specpath artifacts/desktop-control tools/amac_control_app.py
    if ($LASTEXITCODE -ne 0) { throw '桌面控制台打包失败。' }
    $Executable = Join-Path $ProjectRoot 'release/desktop/aMac-Control.exe'
    $GitCommit = (& git rev-parse HEAD).Trim()
    $GitDirty = [bool](& git status --porcelain)
    $Metadata = [ordered]@{
        name = 'aMac-Control'; version = '0.1.0'; platform = 'Windows x64'
        source_commit = $GitCommit; source_dirty = $GitDirty
        sha256 = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant()
        bytes = (Get-Item -LiteralPath $Executable).Length
    }
    $Metadata | ConvertTo-Json | Set-Content -LiteralPath 'release/desktop/aMac-Control.json' -Encoding UTF8
    Write-Host ('已生成：' + $Executable)
} finally { Pop-Location }
