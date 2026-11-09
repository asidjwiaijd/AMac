"""Wire the shared birthday code into the pinned SDL2 desktop emulator."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess

PIN = "f532be3d3d70b70f272b52050ce0889a4025d624"
ROOT = Path(__file__).resolve().parents[1]
HASHES = {
    "BasiliskII/src/SDL/video_sdl2.cpp": "b9e8cd43d12ceccde66483c2339896a09a4a8b7759cd47be6b2471661f63e133",
    "BasiliskII/src/Unix/Makefile.in": "7c839bb038e1903fe370ece1bacf1f5b310aed82b34599f77aff5c94368952f5",
    "BasiliskII/src/emul_op.cpp": "357b0f6713edaaadfe527ca4959080824f39f58d5c014c22d521d6debb416cae",
}

def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Pinned desktop anchor changed: {old[:80]!r}")
    return text.replace(old, new, 1)

def patched(path, text):
    if path.endswith("emul_op.cpp"):
        text = once(text, '#include "emul_op.h"', '#include "emul_op.h"\n#include "birthday_bridge.h"')
        text=once(text, "\t\t\tCheckLoad(type, id, p, size);",
                  "\t\t\tCheckLoad(type, id, p, size);\n\t\t\tAmacBirthdayResource(type, id, p, size);")
        return once(text, "\t\tcase M68K_EMUL_OP_IDLE_TIME:\t// SynchIdleTime() patch",
                    "\t\tcase M68K_EMUL_OP_IDLE_TIME:\t// SynchIdleTime() patch\n\t\t\tAmacBirthdayIdle();")
    if path.endswith("Makefile.in"):
        text = once(text, "CXXFLAGS = @CXXFLAGS@", "CXXFLAGS = @CXXFLAGS@\nCXXFLAGS += -std=c++17")
        text = once(text, "CPPFLAGS = @CPPFLAGS@", "CPPFLAGS = @CPPFLAGS@ -DBIRTHDAY_HOST_TEST=1 -DMAC_SHUTDOWN_HOST_TEST=1 -I@top_srcdir@/../amac")
        text = once(text, "    $(XPLAT_SRCS) $(SYSSRCS) $(CPUSRCS) $(SLIRP_SRCS)",
                    "    ../amac/birthday_bridge.cpp ../amac/birthday_gift.cpp ../amac/birthday_gift_art.cpp \\\n    ../amac/birthday_desktop_control.cpp ../amac/mac_shutdown.cpp \\\n    $(XPLAT_SRCS) $(SYSSRCS) $(CPUSRCS) $(SLIRP_SRCS)")
        return text
    text = once(text, '#include "adb.h"', '#include "adb.h"\n#include "birthday_bridge.h"')
    for name in ("KeyDown", "KeyUp", "MouseDown", "MouseUp"):
        text = re.sub(r"\bADB" + name + r"\b", "AmacDesktop" + name, text)
    text = once(text, "\tclassic_mode = classic;", "\tclassic_mode = classic;\n\tAmacBirthdayInit();")
    text = once(text, "static void delete_sdl_video_window()\n{", "static void delete_sdl_video_window()\n{\n\tAmacBirthdayDestroy();")
    text = once(text, "\tSDL_SetWindowTitle(sdl_window, s.c_str());",
                '\ts += " | aMac Birthday Edition";\n\tSDL_SetWindowTitle(sdl_window, s.c_str());')
    text = once(text, "static int present_sdl_video()\n{", """static int present_sdl_video()
{
    if (AmacBirthdayFrame() && guest_surface) {
        SDL_LockMutex(sdl_update_video_mutex);
        SDL_Rect full = {0, 0, guest_surface->w, guest_surface->h};
        SDL_UnionRect(&sdl_update_video_rect, &full, &sdl_update_video_rect);
        SDL_UnlockMutex(sdl_update_video_mutex);
    }""")
    text = once(text, "\tSDL_RenderPresent(sdl_renderer);",
                "\tif (!AmacBirthdayDraw(sdl_renderer)) return -1;\n\tSDL_RenderPresent(sdl_renderer);")
    text = once(text, "\t\t\tSDL_Event & event = events[i];", """\t\t\tSDL_Event & event = events[i];
            if (AmacBirthdayEvent(event, mouse_grabbed)) {
                ctrl_down = opt_down = cmd_down = false;
                continue;
            }""")
    return text

def prepare(tree):
    revision = subprocess.check_output(["git", "-C", str(tree), "rev-parse", "HEAD"], text=True).strip()
    if revision != PIN:
        raise ValueError(f"Desktop must be pinned at {PIN}, got {revision}")
    for path, digest in HASHES.items():
        original = subprocess.check_output(["git", "-C", str(tree), "show", f"{PIN}:{path}"])
        if hashlib.sha256(original).hexdigest() != digest:
            raise ValueError(f"Unexpected desktop source: {path}")
        result = patched(path, original.decode("utf-8")).encode("utf-8")
        target = tree / path
        if target.read_bytes() not in (original, result):
            raise ValueError(f"Refusing to overwrite unrelated local edits: {target}")
        if target.read_bytes() != result:
            target.write_bytes(result)
    stage = tree / "BasiliskII/src/amac"
    stage.mkdir(parents=True, exist_ok=True)
    files = [ROOT / "examples/amac-desktop" / f"birthday_bridge.{ext}" for ext in ("h", "cpp")]
    files += [ROOT / "examples/amac-desktop/birthday_desktop_control.cpp",
              ROOT / "src/basilisk/mac_shutdown.cpp", ROOT / "src/basilisk/include/mac_shutdown.h"]
    files += [ROOT / "src/basilisk" / f"birthday_gift{suffix}.cpp" for suffix in ("", "_art")]
    files += [ROOT / "src/basilisk/include" / f"{name}.h" for name in ("birthday_gift", "birthday_config", "birthday_guest")]
    files += [ROOT / "src/generated/charcoal_font_data.h"]
    signature = hashlib.sha256(b"".join(source.name.encode()+b"\0"+source.read_bytes() for source in files)).hexdigest()
    stamp = stage / ".shared-inputs.sha256"
    changed = not stamp.exists() or stamp.read_text() != signature
    for source in files:
        target = stage / source.name
        if not target.exists() or target.read_bytes() != source.read_bytes():
            shutil.copyfile(source, target)
    # Upstream's makefile does not generate header dependencies. Invalidate
    # just the adapter consumers whenever any shared input changes.
    if changed:
        for target in [*stage.glob("*.cpp"), tree / "BasiliskII/src/SDL/video_sdl2.cpp",
                       tree / "BasiliskII/src/emul_op.cpp"]:
            target.touch()
        stamp.write_text(signature)
    print(f"Desktop birthday adapter ready; shared firmware code, pinned {PIN}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tree", type=Path)
    prepare(parser.parse_args().tree.resolve())
