"""Exercise the production network clock and persistent RTC with SDK stubs."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NetworkTimeTests(unittest.TestCase):
    def test_network_sync_offline_and_reconnect(self):
        compiler = os.environ.get("CXX") or shutil.which("c++")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / "network-time"
            subprocess.run([
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-DBOARD_AMAC_P4", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-I", str(ROOT / "test/time_stubs"),
                "-I", str(ROOT / "test/clock_stubs"),
                "-I", str(ROOT / "src/basilisk/include"),
                str(ROOT / "src/basilisk/mac_network_time.cpp"),
                str(ROOT / "src/basilisk/mac_clock_esp32.cpp"),
                str(ROOT / "test/test_network_time.cpp"), "-o", str(binary)
            ], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
