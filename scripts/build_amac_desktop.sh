#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
pin=f532be3d3d70b70f272b52050ce0889a4025d624
cache="${XDG_CACHE_HOME:-$HOME/.cache}/amac-birthday-desktop"
tree="$cache/macemu"
mkdir -p "$cache" "$repo/artifacts/desktop"
if [[ ! -d "$tree/.git" ]]; then
    if [[ -d "$repo/artifacts/desktop/macemu/.git" ]]; then
        git -c core.autocrlf=false clone --quiet --shared "$repo/artifacts/desktop/macemu" "$tree"
    else
        git -c core.autocrlf=false clone https://github.com/kanjitalk755/macemu.git "$tree"
    fi
    git -C "$tree" checkout --detach "$pin"
fi
python3 "$repo/scripts/build_assets.py"
python3 "$repo/scripts/patch_desktop_birthday.py" "$tree"
cd "$tree/BasiliskII/src/Unix"
if [[ ! -f Makefile || Makefile.in -nt Makefile ]]; then
    sh ./autogen.sh --without-gtk --enable-sdl-video --enable-sdl-audio \
        --disable-jit-compiler --enable-addressing=direct
fi
make -j"${AMAC_BUILD_JOBS:-6}"
install -m 755 BasiliskII "$repo/artifacts/desktop/BasiliskII-birthday"
sha256sum "$repo/artifacts/desktop/BasiliskII-birthday"
