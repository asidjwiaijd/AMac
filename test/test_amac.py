"""aMac hardware evidence and native display/UI integration checks.

python -m unittest discover -s test -p test_amac.py -v
Set CXX and AMAC_SANITIZERS=address,undefined for an ASan-capable host compiler.
PNG previews are written to artifacts/amac-ui/ by the UI integration test.
"""
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mac_control import encode_png


def netlist(path):
    text = path.read_text(encoding="utf-8").split("$NETS", 1)[1].split("$SCHEDULE", 1)[0]
    text = re.sub(r",\s*", " ", text)
    return {name.strip().strip("'"): set(pads.split())
            for name, pads in re.findall(r"([^;\n]+);([^\n]*)", text)}


class HardwareTests(unittest.TestCase):
    def test_supplied_panel_records_and_timing(self):
        source = ROOT / "hardware/reference/tspi-rk3566-dsi-v10.dtsi"
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),
                         "337b30a225c043c4aa904b9bd92ed343390ea7197e4ac228517b5644d94db088")
        subprocess.run([sys.executable, str(ROOT / "scripts/generate_hd20034_profile.py"), "--check"], check=True)
        block = source.read_text(encoding="utf-8").split("panel-init-sequence = [", 1)[1].split("]", 1)[0]
        lines = (line.split("//", 1)[0].strip() for line in block.splitlines())
        expected = [bytes.fromhex(line) for line in lines if line]
        profile = (ROOT / "src/board/amac/hd20034_panel_profile.h").read_text()
        # Independent, line-based reference parser checks the generated data
        # byte-for-byte, including packet type, command lengths and delays.
        actual = []
        for command, length, delay, data in re.findall(
                r"\{0x([0-9A-F]{2}), (\d+), (\d+), \{([^}]*)\}\}", profile):
            length, delay = int(length), int(delay)
            payload = [int(b, 16) for b in data.split(",")][:length]
            packet = 0x05 if length == 0 else 0x15 if length == 1 else 0x39
            actual.append(bytes([packet, delay, length + 1, int(command, 16), *payload]))
        self.assertEqual(actual, expected)
        self.assertEqual(len(actual), 41)
        self.assertEqual(sum(row[1] for row in actual), 730)
        for name, prop in {"HFrontPorch": "hfront-porch", "HBackPorch": "hback-porch",
                           "HSync": "hsync-len", "VFrontPorch": "vfront-porch",
                           "VBackPorch": "vback-porch", "VSync": "vsync-len",
                           "LaneRateMbps": "rockchip,lane-rate", "DataLanes": "dsi,lanes",
                           "PixelClockHz": "clock-frequency"}.items():
            value = int(re.search(re.escape(prop) + r"\s*=\s*<(\d+)>", source.read_text())[1])
            self.assertRegex(profile, rf"k{name} = {value};")

    def test_netlist_pads_and_gpio_map(self):
        bottom = netlist(ROOT / "hardware/netlists/Netlist_sch_bottom_2026-11-02.tel")
        front = netlist(ROOT / "hardware/netlists/Netlist_sch_front_2026-11-02-1.tel")
        for net, pads in {
            "LCD_BKL_EN": {"U3.28", "FPC2.2"},
            "MIPI_DSI_RESET": {"U3.32", "R23.1"},
            "PWM15_IR_M0": {"U3.27", "FPC2.5"},
            "GPIO22": {"U3.31", "R34.1", "R35.2"},
            "IS_CHARGING": {"U3.30", "U9.1"},
            "GPIO45": {"U3.73", "U6.9"},
        }.items():
            self.assertTrue(pads <= bottom[net], (net, pads))
        pins = (ROOT / "src/board/amac/amac_pins.h").read_text()
        for name, value in {"LCD_RESET": 23, "LCD_BACKLIGHT": 13, "IR": 12,
                            "SD_CLK": 43, "SD_CMD": 44, "SD_D0": 39, "SD_D1": 40,
                            "SD_D2": 41, "SD_D3": 42, "SD_DETECT": 45,
                            "WIFI_CLK": 18, "WIFI_CMD": 19, "WIFI_D0": 14,
                            "WIFI_D1": 15, "WIFI_D2": 16, "WIFI_D3": 17,
                            "WIFI_RESET": 54}.items():
            self.assertRegex(pins, rf"#define\s+AMAC_{name}_GPIO\s+{value}\b")
        for net, module, b, f, panel in [
            ("MIPI_DSI_0P", 38, 20, 12, 35), ("MIPI_DSI_0N", 39, 21, 11, 36),
            ("MIPI_DSI_CLKP", 37, 17, 15, 32), ("MIPI_DSI_CLKN", 36, 18, 14, 33),
            ("MIPI_DSI_1P", 34, 14, 18, 29), ("MIPI_DSI_1N", 35, 15, 17, 30),
        ]:
            self.assertEqual(b + f, 32)
            self.assertTrue({f"U3.{module}", f"FPC2.{b}"} <= bottom[net])
            self.assertTrue({f"FPC2.{f}", f"FPC1.{panel}"} <= front[net])
        # All 31 inter-board conductors must match after reversal, including
        # unlabeled pass-throughs; pins 32/33 are connector mounting pads.
        find = lambda nets, pad: next((n for n, pads in nets.items() if pad in pads), None)
        for b in range(1, 32):
            bn, fn = find(bottom, f"FPC2.{b}"), find(front, f"FPC2.{32-b}")
            if bn is None and fn is None:  # intentionally unconnected conductor
                continue
            self.assertIsNotNone(bn, f"bottom FPC2.{b}")
            self.assertIsNotNone(fn, f"front FPC2.{32-b}")
            if bn in ("VCC", "VCC_3V3", "GND"):
                self.assertEqual(bn, fn)
        variant = (ROOT / "variants/amac_p4c5/pins_arduino.h").read_text()
        self.assertNotRegex(variant, r"#define\s+BOARD_SDMMC_POWER_PIN\b")
        self.assertRegex(variant, r"#define\s+BOARD_SDMMC_POWER_CHANNEL\s+-1\b")

    def test_no_c6_image_or_m5_audio_in_amac_build(self):
        from configparser import ConfigParser
        cfg = ConfigParser(interpolation=None)
        cfg.read(ROOT / "platformio.ini")
        target = cfg["env:amac_p4_rev3"]
        self.assertNotIn("fetch_c6", target["extra_scripts"])
        self.assertIn("-<board/tab5/*>", target["build_src_filter"])
        self.assertIn("-<basilisk/audio_esp32.cpp>", target["build_src_filter"])
        self.assertEqual(target["board_build.variant"], "amac_p4c5")

    def test_c5_transport_is_the_matching_upstream_version(self):
        source = ROOT / "third_party/esp_hosted_2_12_3/transport_drv.c"
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),
                         "6880fc3fe2a8e9c8103f79eece9cda9b586a33a75b72029dc65b3021a5214d5c")


class NativeIntegrationTests(unittest.TestCase):
    def compile_run(self, source, board, extras, output=None, include_dirs=(), flags=()):
        compiler = os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
        self.assertIsNotNone(compiler, "Install a host C++17 compiler or set CXX")
        temp_root = ROOT / "artifacts/tmp"
        temp_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_root) as folder:
            binary = Path(folder) / ("test.exe" if os.name == "nt" else "test")
            args = [compiler, "-std=c++17", "-O1", "-g", "-pthread", f"-D{board}=1"]
            args += list(flags)
            sanitizer = os.environ.get("AMAC_SANITIZERS")
            if sanitizer:
                args += [f"-fsanitize={sanitizer}", "-fno-omit-frame-pointer"]
            for directory in [*include_dirs, "test/display_stubs", "src/board", "src/basilisk",
                              "src/basilisk/include", "src/generated"]:
                args += ["-I", str(ROOT / directory)]
            args += [str(ROOT / source), *[str(ROOT / p) for p in extras], "-o", str(binary)]
            subprocess.run(args, check=True, cwd=ROOT)
            env = dict(os.environ, PREBOOT_UI_OUT=folder)
            subprocess.run([str(binary)], check=True, timeout=60, env=env)
            if output:
                output.mkdir(parents=True, exist_ok=True)
                for ppm in Path(folder).glob("*.ppm"):
                    header, _, pixels = ppm.read_bytes().partition(b"\n255\n")
                    w, h = map(int, header.split()[1:3])
                    (output / (ppm.stem + ".png")).write_bytes(encode_png(w, h, pixels))

    def test_display_all_depths_and_legacy_boards(self):
        for board in ["BOARD_AMAC_P4", "BOARD_M5STACK_TAB5", "BOARD_WAVESHARE_P4_101"]:
            with self.subTest(board=board):
                extras = ["src/board/board_display_surface.cpp", "src/board/mini_gfx/mini_gfx.cpp"]
                if board == "BOARD_AMAC_P4":
                    extras += ["src/basilisk/birthday_gift.cpp", "src/basilisk/birthday_gift_art.cpp",
                               "src/basilisk/birthday_gift_amac.cpp"]
                self.compile_run("test/test_display_pipeline.cpp", board, extras,
                                 flags=["-DBIRTHDAY_HOST_TEST=1"] if board == "BOARD_AMAC_P4" else [])

    def test_birthday_secrets_and_art(self):
        subprocess.run([sys.executable, str(ROOT / "scripts/build_assets.py")], check=True, cwd=ROOT)
        self.compile_run("test/test_birthday_gift.cpp", "BOARD_AMAC_P4",
                         ["src/basilisk/birthday_gift.cpp", "src/basilisk/birthday_gift_art.cpp"],
                         ROOT / "artifacts/birthday")

    def test_birthday_input_handover(self):
        self.compile_run("test/test_birthday_input.cpp", "BOARD_AMAC_P4",
                         ["src/basilisk/birthday_gift.cpp", "src/basilisk/birthday_gift_art.cpp",
                          "src/basilisk/birthday_gift_amac.cpp"], flags=["-DBIRTHDAY_HOST_TEST=1"])

    def test_amac_panel_bringup(self):
        self.compile_run("test/test_amac_panel_driver.cpp", "BOARD_AMAC_P4",
                         ["src/board/amac/board_display_amac.cpp", "src/board/board_display_surface.cpp",
                          "src/board/mini_gfx/mini_gfx.cpp"], include_dirs=["test/panel_stubs"])

    def test_battery_voltage_policy_and_adc(self):
        self.compile_run("test/test_amac_battery.cpp", "BOARD_AMAC_P4",
                         ["src/board/amac/board_power_amac.cpp"], include_dirs=["test/power_stubs"])

    def test_mac_clock_survives_deep_sleep(self):
        self.compile_run("test/test_amac_clock_sleep.cpp", "BOARD_AMAC_P4", [],
                         include_dirs=["test/clock_stubs"])

    def test_preboot_layout_and_panel_pixels(self):
        subprocess.run([sys.executable, str(ROOT / "scripts/build_assets.py")], check=True, cwd=ROOT)
        for board in ["BOARD_AMAC_P4", "BOARD_M5STACK_TAB5", "BOARD_WAVESHARE_P4_101"]:
            with self.subTest(board=board):
                self.compile_run("test/test_preboot_ui.cpp", board,
                                 ["src/basilisk/preboot_ui.cpp", "src/basilisk/preboot_screens.cpp",
                                  "src/board/board_display_surface.cpp", "src/board/mini_gfx/mini_gfx.cpp"],
                                 ROOT / "artifacts/amac-ui" / board)


if __name__ == "__main__":
    unittest.main()
