#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
tools="$repo/artifacts/desktop/remote-tools"
mkdir -p "$tools/debs" "$tools/root"
if [[ ! -x "$tools/root/usr/bin/x11vnc" || ! -f "$tools/root/usr/share/novnc/vnc.html" ]]; then
    cd "$tools/debs"
    apt-get download x11vnc novnc libvncserver1 libvncclient1 python3-websockify websockify
    for package in ./*.deb; do dpkg-deb -x "$package" "$tools/root"; done
fi
command -v Xvfb >/dev/null
command -v xdpyinfo >/dev/null
echo 'Local browser display tools ready (extracted inside the project).'
