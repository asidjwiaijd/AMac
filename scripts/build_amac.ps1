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
# Preserve the existing publishable image before a build can replace it.
# The USB updater can use this archive as a rollback copy after matching it
# against the target's own ROM flash digest (without streaming flash reads).
function Get-AmacFileSha256([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $sha.Dispose() }
}
$releaseStem = if ($Environment -eq 'amac_p4_es') { 'aMac-P4C5-HD20034-ES' } else { 'aMac-P4C5-HD20034-Rev3' }
$releaseDirectory = Join-Path $ProjectRoot 'release'
$existingBinary = Join-Path $releaseDirectory ($releaseStem + '.bin')
$existingMetadata = Join-Path $releaseDirectory ($releaseStem + '.json')
if ((Test-Path -LiteralPath $existingBinary) -and (Test-Path -LiteralPath $existingMetadata)) {
    $releaseMetadata = Get-Content -LiteralPath $existingMetadata -Raw | ConvertFrom-Json
    $releaseHash = Get-AmacFileSha256 $existingBinary
    if ($releaseHash -ne $releaseMetadata.merged_sha256) { throw 'Existing release hash does not match its metadata.' }
    $archiveDirectory = Join-Path (Join-Path $releaseDirectory 'archive\builds') $releaseHash.Substring(0, 16)
    New-Item -ItemType Directory -Force -Path $archiveDirectory | Out-Null
    $archivedBinary = Join-Path $archiveDirectory ($releaseStem + '.bin')
    $archivedMetadata = Join-Path $archiveDirectory ($releaseStem + '.json')
    if (-not (Test-Path -LiteralPath $archivedBinary)) { Copy-Item -LiteralPath $existingBinary -Destination $archivedBinary }
    if (-not (Test-Path -LiteralPath $archivedMetadata)) { Copy-Item -LiteralPath $existingMetadata -Destination $archivedMetadata }
    if ((Get-AmacFileSha256 $archivedBinary) -ne $releaseHash) { throw 'Archived release verification failed.' }
    Write-Host ('[ARCHIVE] Verified previous release: ' + $archiveDirectory)
}
& platformio run --project-dir $ProjectRoot --environment $Environment
exit $LASTEXITCODE
