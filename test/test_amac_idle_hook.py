"""Exercise the real resource patch and idle opcode with idlewait disabled."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class IdleShutdownHookTests(unittest.TestCase):
    def test_shutdown_hook_is_independent_of_idle_yield(self):
        source = (ROOT / 'src/basilisk/rsrc_patches.cpp').read_text()
        start = source.index('static uint32 find_rsrc_data(')
        end = source.index('void CheckLoad(', start)
        patch = source[start:end]
        source = (ROOT / 'src/basilisk/emul_op.cpp').read_text()
        start = source.index('\t\tcase M68K_EMUL_OP_IDLE_TIME:')
        end = source.index('\t\tcase M68K_EMUL_OP_POWER_GESTALT:', start)
        opcode = source[start:end]
        fixture = (ROOT / 'test/test_amac_idle_hook.cpp').read_text()
        fixture = fixture.replace('// INSERT_PATCH', patch).replace('// INSERT_OPCODE', opcode)
        compiler = os.environ.get('CXX') or shutil.which('c++')
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp) / 'idle.cpp'
            cpp.write_text(fixture)
            for amac in (False, True):
                binary = Path(temp) / ('amac' if amac else 'other-board')
                subprocess.run([
                    compiler, '-std=c++17', '-O1', '-g',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    *(['-DBOARD_AMAC_P4=1'] if amac else []),
                    str(cpp), '-o', str(binary)
                ], check=True)
                subprocess.run([str(binary)], check=True, timeout=10)


if __name__ == '__main__':
    unittest.main()
