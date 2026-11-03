param(
    [ValidateSet('amac_p4_rev3', 'amac_p4_es')]
    [string]$Environment = 'amac_p4_rev3'
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if (-not $env:PLATFORMIO_CORE_DIR) {
    $env:PLATFORMIO_CORE_DIR = Join-Path $ProjectRoot '.pio-core'
}
# Large SDK downloads and extraction stay beside the project, including temp files.
$env:TEMP = Join-Path $env:PLATFORMIO_CORE_DIR 'tmp'
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
& platformio run --project-dir $ProjectRoot --environment $Environment
exit $LASTEXITCODE
