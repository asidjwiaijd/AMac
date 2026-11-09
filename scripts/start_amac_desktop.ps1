param([switch]$NoBrowser)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$LogRoot = Join-Path $ProjectRoot 'artifacts\desktop\browser'
New-Item -ItemType Directory -Force $LogRoot | Out-Null
$PreviewUrl = 'http://127.0.0.1:6083/vnc.html?autoconnect=1&resize=scale&show_dot=0'
function Test-PreviewReady {
    $PreviewSocket = [System.Net.Sockets.TcpClient]::new()
    try {
        $Response = Invoke-WebRequest -Uri 'http://127.0.0.1:6083/vnc.html' -UseBasicParsing -TimeoutSec 1
        if ($Response.StatusCode -ne 200 -or $Response.Content -notmatch 'noVNC') { return $false }
        if (-not $PreviewSocket.ConnectAsync('127.0.0.1',5903).Wait(700)) { return $false }
        $PreviewSocket.ReceiveTimeout = 700
        $Banner = [byte[]]::new(12)
        $Count = $PreviewSocket.GetStream().Read($Banner,0,12)
        return $Count -ge 4 -and [System.Text.Encoding]::ASCII.GetString($Banner,0,4) -eq 'RFB '
    } catch { return $false }
    finally { $PreviewSocket.Dispose() }
}
$Ready = Test-PreviewReady
if (-not $Ready) {
    Start-Process -FilePath 'wsl.exe' -ArgumentList @('-d','Ubuntu','--cd',$ProjectRoot,'--','bash','scripts/run_amac_browser.sh') `
        -WindowStyle Hidden -RedirectStandardOutput (Join-Path $LogRoot 'launcher.log') `
        -RedirectStandardError (Join-Path $LogRoot 'launcher.err.log') | Out-Null
    for ($Attempt=0; $Attempt -lt 40; $Attempt++) {
        $Ready = Test-PreviewReady
        if ($Ready) { break }
        Start-Sleep -Milliseconds 250
    }
}
if (-not $Ready) { throw "Mac preview did not start. See $LogRoot\launcher.err.log and the browser logs." }
if (-not $NoBrowser) { Start-Process $PreviewUrl }
Write-Output $PreviewUrl
