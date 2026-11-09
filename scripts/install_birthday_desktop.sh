#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
desktop="$repo/artifacts/desktop"
tools="$desktop/probe/tools/usr/bin"
if ! command -v hmount >/dev/null; then export PATH="$tools:$PATH"; fi
python3 "$repo/guest/birthday/package.py"
exec 9>"$desktop/.session.lock"
flock -n 9 || { echo 'Close the running Mac normally before installing.' >&2; exit 73; }
mkdir -p "$desktop/backup/before-birthday-icons"
for name in Macintosh7.dsk Macintosh8.dsk; do
    image="$desktop/$name"
    [[ -f "$image" ]] || continue
    backup="$desktop/backup/before-birthday-icons/$name"
    if [[ ! -f "$backup" ]]; then
        cp --reflink=auto -- "$image" "$backup"
        cmp -- "$image" "$backup"
    fi
    hmount "$image"
    trap 'humount >/dev/null 2>&1 || true' EXIT
    exists() {
        local parent=${1%:*} leaf=${1##*:}
        parent=${parent:-:}
        # hls can return success for an unmatched filename pattern. Inspect
        # the parent listing instead so a missing application is installed.
        hls -1a "$parent" | grep -Fx -- "$leaf" >/dev/null
    }
    for folder in ':Desktop Folder' ':System Folder:Apple Menu Items' ':System Folder:Control Panels'; do
        exists "$folder" || hmkdir "$folder"
    done
    install_one() {
        local file=$1 target=$2
        if exists "$target"; then
            local audit="$desktop/backup/before-birthday-icons/$name-$file.bin" status=0
            hcopy -m "$target" "$audit"
            python3 "$repo/guest/birthday/package.py" --compare "$audit" "$repo/guest/birthday/dist/$file.bin" || status=$?
            if [[ $status -eq 1 ]]; then
                hcopy -m "$repo/guest/birthday/dist/$file.bin" "$target"
                echo "Updated owned birthday launcher: $target"
            elif [[ $status -eq 0 ]]; then
                echo "Already current: $target"
            else
                return "$status"
            fi
        else
            hcopy -m "$repo/guest/birthday/dist/$file.bin" "$target"
        fi
    }
    install_one 'For You' ':Desktop Folder:For You'
    install_one 'Start Here' ':Desktop Folder:Start Here'
    install_one 'Birthday Settings' ':System Folder:Control Panels:Birthday Settings'
    install_one 'Pocket Terminal' ':System Folder:Apple Menu Items:Pocket Terminal'
    install_one 'About This Gift' ':System Folder:Apple Menu Items:About This Gift'
    humount "$image"
    trap - EXIT
    echo "Installed birthday applications in $name; original image backed up."
done
