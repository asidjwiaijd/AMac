#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
desktop="$repo/artifacts/desktop"
tools="$desktop/remote-tools/root"
for file in BasiliskII-birthday Q650.ROM Macintosh7.dsk; do
    [[ -f "$desktop/$file" ]] || { echo "Missing $file; build/prepare the desktop preview first." >&2; exit 1; }
done
[[ -f "$desktop/basilisk_ii_prefs" ]] || cp "$repo/examples/amac-desktop/basilisk_ii_prefs" "$desktop/basilisk_ii_prefs"
bash "$repo/scripts/prepare_amac_browser.sh"
exec 9>"$desktop/.session.lock"
flock -n 9 || { echo 'The Mac preview is already running.' >&2; exit 73; }
cd "$desktop"
mkdir -p browser
export DISPLAY=:93 SDL_VIDEODRIVER=x11 SDL_RENDER_DRIVER=software
unset WAYLAND_DISPLAY
export XDG_SESSION_TYPE=x11
export LD_LIBRARY_PATH="$tools/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PYTHONPATH="$tools/usr/lib/python3/dist-packages${PYTHONPATH:+:$PYTHONPATH}"
owned=()
cleanup() {
    for ((index=${#owned[@]}-1;index>=0;index--)); do
        kill "${owned[index]}" 2>/dev/null || true
    done
    for ((attempt=0;attempt<10;attempt++)); do
        local alive=false
        for process in "${owned[@]}"; do kill -0 "$process" 2>/dev/null && alive=true; done
        $alive || break
        sleep .1
    done
    # Some libvncserver builds can hang in XIO cleanup after a display closes.
    # These are the exact child PIDs created by this launcher.
    for process in "${owned[@]}"; do kill -KILL "$process" 2>/dev/null || true; done
    wait 2>/dev/null || true
}
trap cleanup EXIT
Xvfb :93 -screen 0 960x720x24 -nolisten tcp -ac > browser/xvfb.log 2>&1 9>&- &
owned+=("$!")
for ((i=0;i<50;i++)); do xdpyinfo -display :93 >/dev/null 2>&1 && break; sleep .1; done
# WSL can provide only X11's abstract socket; a pathname test rejects a
# healthy display. Check the display protocol itself instead.
kill -0 "${owned[0]}" 2>/dev/null && xdpyinfo -display :93 >/dev/null 2>&1 || {
    cat browser/xvfb.log >&2; exit 1;
}
"$tools/usr/bin/x11vnc" -display :93 -listen 127.0.0.1 -rfbport 5903 -forever -shared \
    -nopw -noshm -noxdamage > browser/vnc.log 2>&1 9>&- &
owned+=("$!")
python3 -m websockify --web "$tools/usr/share/novnc" 127.0.0.1:6083 127.0.0.1:5903 \
    > browser/web.log 2>&1 9>&- &
owned+=("$!")
python3 - "${owned[@]}" <<'PY'
import os,socket,sys,time,urllib.request
deadline=time.monotonic()+8
while True:
    for pid in map(int,sys.argv[1:]):
        os.kill(pid,0)
    try:
        with socket.create_connection(('127.0.0.1',5903),.3) as peer:
            peer.settimeout(.3)
            assert peer.recv(12).startswith(b'RFB ')
        with urllib.request.urlopen('http://127.0.0.1:6083/vnc.html',timeout=.3) as response:
            assert response.status==200
        break
    except (OSError,AssertionError):
        if time.monotonic()>=deadline: raise RuntimeError('Local Mac display did not become ready')
        time.sleep(.1)
PY
echo 'aMac browser preview: http://127.0.0.1:6083/vnc.html?autoconnect=1&resize=scale&show_dot=0'
stdbuf -oL ./BasiliskII-birthday --config ./basilisk_ii_prefs --nogui \
    > browser/mac.log 2> browser/mac.err.log &
mac_process=$!
echo "$mac_process" > browser/mac.pid
wait "$mac_process"
