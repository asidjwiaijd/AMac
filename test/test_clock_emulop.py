"""Replay ROM clock writes through the actual production ClkNoMem case."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ClockEmulOpTests(unittest.TestCase):
    def test_q650_clock_and_pram_write_abi(self):
        source = (ROOT / "src/basilisk/emul_op.cpp").read_text()
        begin = source.index("\t\tcase M68K_EMUL_OP_CLKNOMEM:")
        end = source.index("\t\tcase M68K_EMUL_OP_READ_XPRAM:", begin)
        compiler = os.environ.get("CXX") or shutil.which("c++")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            (directory / "clock_emulop_case.inc").write_text(source[begin:end])
            binary = directory / "clock-emulop"
            subprocess.run([
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-I", str(directory), "-I", str(ROOT / "test/clock_stubs"),
                "-I", str(ROOT / "src/basilisk/include"),
                str(ROOT / "src/basilisk/mac_clock_esp32.cpp"),
                str(ROOT / "test/test_clock_emulop.cpp"), "-o", str(binary)
            ], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
