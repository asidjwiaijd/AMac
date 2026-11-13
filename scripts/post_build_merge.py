"""
Post-build script to create merged release binary.

This script runs after PlatformIO builds the firmware and creates
a properly merged binary with bootloader, partition table, and
application - ready for single-command flashing.

The merged binary is created in the project's 'release' directory.

To enable, add to platformio.ini:
    extra_scripts =
        post:scripts/post_build_merge.py
"""

Import("env")

import os
import subprocess
import sys
import json
import re
import hashlib
from pathlib import Path

sys.path.insert(0, os.path.join(env.subst("$PROJECT_DIR"), "scripts"))
from release_images import TARGETS, validate_merged
from patch_amac_bootloader import patch_bootloader


def prepare_amac_bootloader(source, target, env):
    if env["PIOENV"] != "amac_p4_rev3":
        return
    path = Path(env.subst("$BUILD_DIR"), "bootloader.bin")
    original = path.read_bytes()
    patched = patch_bootloader(original)
    if patched != original:
        path.write_bytes(patched)
        print("[BOOT] Applied guarded P4 rev3 DCDC startup wait: 2000 us")

def create_merged_binary(source, target, env):
    """Create a merged binary after successful build."""
    if env["PIOENV"] in ("esp32p4_pioarduino_debug", "esp32p4_aot_profile", "esp32p4_aot_selftest"):
        print("[MERGE] Debug firmware stays in its build directory; release images unchanged")
        return
    
    project_dir = env.subst("$PROJECT_DIR")
    build_dir = env.subst("$BUILD_DIR")

    if env["PIOENV"].startswith("amac_p4_"):
        link_map = Path(build_dir, "firmware.map").read_text(errors="replace")
        if "esp_hosted_transport_amac.c.o" not in link_map:
            raise RuntimeError("aMac C5 transport object is missing from the link map")
        if re.search(r"libesp_hosted\.a\(transport_drv\.c\.(?:obj|o)\)", link_map):
            raise RuntimeError("SDK C6 transport was linked into aMac; reject this image")
    
    # Prebuilt P4 image headers do not reliably encode their minimum revision.
    # Verify the actual linked SDK before labeling an image for download.
    variant = env.BoardConfig().get("build.chip_variant", "esp32p4")
    package = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs"))
    sdkconfig = (package / variant / "sdkconfig").read_text()
    revision = re.search(r"^CONFIG_ESP32P4_REV_MIN_FULL=(\d+)$", sdkconfig, re.M)
    if not revision:
        raise RuntimeError("Cannot verify SDK silicon revision")
    minimum_revision = int(revision.group(1))
    production = env["PIOENV"].endswith("_rev3")
    if (production and minimum_revision != 301) or (not production and minimum_revision >= 300):
        raise RuntimeError(f"Wrong SDK for {env['PIOENV']}: minimum revision {minimum_revision}")

    # File paths
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware = os.path.join(build_dir, "firmware.bin")
    
    # Output directory and file
    release_dir = os.path.join(project_dir, "release")
    output_file = os.path.join(release_dir, TARGETS[env["PIOENV"]] + ".bin")
    
    # Verify all required files exist
    missing = []
    for f in [bootloader, partitions, firmware]:
        if not os.path.exists(f):
            missing.append(os.path.basename(f))
    
    if missing:
        print(f"[MERGE] Skipping merge - missing files: {', '.join(missing)}")
        return

    if env["PIOENV"] == "amac_p4_rev3":
        original = Path(bootloader).read_bytes()
        patched = patch_bootloader(original)
        if patched != original:
            Path(bootloader).write_bytes(patched)
            print("[BOOT] Applied guarded P4 rev3 DCDC startup wait: 2000 us")
    
    # Create release directory
    os.makedirs(release_dir, exist_ok=True)
    
    # Find esptool - check multiple locations
    esptool_cmd = None
    
    # Possible esptool locations
    possible_paths = [
        os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy") or "", "esptool.py"),
        # PlatformIO's esptool
        os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py"),
        # Homebrew (macOS)
        "/opt/homebrew/bin/esptool.py",
        "/opt/homebrew/bin/esptool",
        "/usr/local/bin/esptool.py",
        "/usr/local/bin/esptool",
    ]
    
    # Check explicit paths first
    for path in possible_paths:
        if os.path.exists(path):
            esptool_cmd = path
            break
    
    # Try PATH as fallback
    if not esptool_cmd:
        for cmd in ["esptool", "esptool.py"]:
            try:
                subprocess.run([cmd, "--version"], capture_output=True, check=True)
                esptool_cmd = cmd
                break
            except (subprocess.CalledProcessError, FileNotFoundError):
                continue
    
    if not esptool_cmd:
        print("[MERGE] WARNING: esptool not found, skipping merge")
        print("[MERGE] Install with: pip install esptool")
        return
    
    print(f"[MERGE] Using esptool: {esptool_cmd}")
    
    # ESP32-P4 flash offsets
    bootloader_offset = "0x2000"
    partition_offset = "0x8000"
    app_offset = "0x10000"
    
    # Build esptool merge command
    # Use older syntax (merge_bin) for compatibility with PlatformIO's bundled esptool
    # pioarduino installs esptool dependencies into its own Python venv.
    # SCons' sys.executable belongs to PlatformIO and may not have rich-click.
    merge_python = (env.PioPlatform().setup_python_env(env)[0]
                    if hasattr(env.PioPlatform(), "setup_python_env") else sys.executable)
    cmd = [
        merge_python, esptool_cmd, "--chip", "esp32p4", "merge_bin",
        "-o", output_file,
        bootloader_offset, bootloader,
        partition_offset, partitions,
        app_offset, firmware
    ]
    
    # If esptool is not a .py file, don't use python to run it
    if not esptool_cmd.endswith(".py"):
        cmd = cmd[1:]  # Remove sys.executable
    
    print("")
    print("[MERGE] Creating merged release binary...")
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            validate_merged(Path(output_file), Path(build_dir))
            metadata = {
                "environment": env["PIOENV"], "chip_variant": variant,
                "minimum_chip_revision": minimum_revision,
                "merged_sha256": hashlib.sha256(Path(output_file).read_bytes()).hexdigest(),
                "components": {Path(path).name: hashlib.sha256(Path(path).read_bytes()).hexdigest()
                               for path in (bootloader, partitions, firmware)},
            }
            git_head = subprocess.run(["git", "-C", project_dir, "rev-parse", "HEAD"],
                                      capture_output=True, text=True)
            git_status = subprocess.run(["git", "-C", project_dir, "status", "--porcelain"],
                                        capture_output=True, text=True)
            metadata["source_commit"] = git_head.stdout.strip() if git_head.returncode == 0 else None
            metadata["source_dirty"] = bool(git_status.stdout.strip()) if git_status.returncode == 0 else None
            if env["PIOENV"] == "amac_p4_rev3":
                metadata["bootloader_workaround"] = "aMac P4 rev3 DCDC settling wait 1000 -> 2000 us"
            if env["PIOENV"].startswith("amac_p4_"):
                metadata["wifi_coprocessor"] = "esp32c5"
                metadata["hosted_transport_version"] = "2.12.3 (source override)"
            Path(build_dir, "release-build.json").write_text(json.dumps(metadata, indent=2) + "\n")
            Path(release_dir, TARGETS[env["PIOENV"]] + ".json").write_text(json.dumps(metadata, indent=2) + "\n")
            # Verify the output
            size = os.path.getsize(output_file)
            print(f"[MERGE] Created: {output_file}")
            print(f"[MERGE] Size: {size:,} bytes ({size / 1024 / 1024:.2f} MB)")
            
            # Verify bootloader header
            with open(output_file, "rb") as f:
                f.seek(0x2000)
                header = f.read(4)
                if header[0] == 0xE9:
                    print("[MERGE] Bootloader header: VALID")
                else:
                    print(f"[MERGE] WARNING: Unexpected bootloader header: {header.hex()}")
        else:
            raise RuntimeError(result.stderr)
            
    except Exception as e:
        raise RuntimeError(f"Failed to create merged firmware: {e}") from e

# Register post-build action
if env["PIOENV"] == "amac_p4_rev3":
    # Also fix the framework's factory image, which is emitted before our merge.
    env.Depends("$BUILD_DIR/firmware.bin", "$BUILD_DIR/bootloader.bin")
    env.AddPreAction("$BUILD_DIR/firmware.bin", prepare_amac_bootloader)
env.AddPostAction("$BUILD_DIR/firmware.bin", create_merged_binary)
