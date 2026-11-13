"""Run production power sequencing with hardware and nonreturning sleep stand-ins."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ShutdownSequencingTests(unittest.TestCase):
    def test_shutdown_and_charger_wake(self):
        source = (ROOT / 'src/basilisk/low_battery_amac.cpp').read_text()
        # Keep all production bodies/state; only replace unavailable SDK headers.
        source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
        fixture = (ROOT / 'test/test_amac_shutdown.cpp').read_text().replace('// INSERT_PRODUCTION', source)
        compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if os.environ.get('AMAC_SANITIZERS') else []
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / 'shutdown.cpp'
            cpp.write_text(fixture)
            binary = Path(folder) / 'shutdown'
            subprocess.run([compiler, '-std=c++17', '-O1', '-g', '-DBOARD_AMAC_P4=1', *flags,
                            '-I', str(ROOT / 'src/basilisk/include'), '-I', str(ROOT / 'src/board'),
                            str(cpp), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=15)
