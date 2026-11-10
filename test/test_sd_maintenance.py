"""Transfer/rollback tests on the production SD backend and Python USB client."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mac_control import MacControl, ControlError


class SdMaintenanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.build.name)/'sd-maintenance'
        compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        flags = ['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.environ.get('AMAC_SANITIZERS') else []
        subprocess.run([compiler,'-std=c++17','-O1','-g',*flags,'-I',str(ROOT/'src/basilisk/include'),
                        str(ROOT/'test/test_sd_maintenance.cpp'),str(ROOT/'src/basilisk/sd_maintenance.cpp'),
                        '-o',str(cls.binary)],check=True)

    @classmethod
    def tearDownClass(cls): cls.build.cleanup()

    def setUp(self):
        self.folder=tempfile.TemporaryDirectory()
        self.root=Path(self.folder.name)
        self.process=None
        self.start()

    def start(self):
        self.process=subprocess.Popen([str(self.binary),str(self.root)],stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE,text=True)
        owner=self
        class Device(MacControl):
            def __init__(self): pass
            def request(self,command,expected,timeout=5):
                reply=owner.command(command.removeprefix('SD '))
                if reply.startswith('@B2 ERR'): raise ControlError(reply)
                if not reply.startswith('@B2 '+expected): raise ControlError(reply)
                return reply
        self.control=Device()

    def stop(self):
        if self.process and self.process.poll() is None:
            self.process.stdin.close();self.assertEqual(self.process.wait(timeout=5),0)
        if self.process: self.process.stdout.close()

    def tearDown(self): self.stop();self.folder.cleanup()

    def command(self,text):
        self.process.stdin.write(text+'\n');self.process.stdin.flush()
        line=self.process.stdout.readline().strip()
        self.assertTrue(line, 'backend crashed')
        return line

    def test_binary_roundtrip_and_previous_file(self):
        original=b'previous configuration'
        (self.root/'settings.txt').write_bytes(original)
        source=self.root/'source.bin';source.write_bytes(bytes(range(256))*31+b'\x00\xffend')
        result=self.control.sd_put(source,'settings.txt')
        self.assertEqual(result['crc32'],zlib.crc32(source.read_bytes()))
        self.assertEqual((self.root/'settings.txt.previous').read_bytes(),original)
        self.assertEqual((self.root/'settings.txt').read_bytes(),source.read_bytes())
        output=self.root/'download.bin';self.control.sd_get('settings.txt',output)
        self.assertEqual(output.read_bytes(),source.read_bytes())
        self.assertIn('settings.txt',self.control.sd_list())
        self.control.sd_get('settings.txt.previous',self.root/'previous-copy')
        self.assertEqual((self.root/'previous-copy').read_bytes(),original)

    def test_bad_crc_offset_and_partial_upload_preserve_original(self):
        (self.root/'safe.txt').write_bytes(b'original')
        name='safe.txt'.encode().hex();crc=zlib.crc32(b'abc')
        self.assertIn('OK SD BEGIN',self.command(f'BEGIN {name} 3 {crc:08x}'))
        self.assertIn('crc_mismatch',self.command('WRITE 0 616263 00000000'))
        self.assertIn('wrong_offset',self.command(f'WRITE 1 616263 {crc:08x}'))
        self.assertIn('incomplete',self.command('COMMIT'))
        self.assertEqual((self.root/'safe.txt').read_bytes(),b'original')
        self.command('ABORT')
        self.assertFalse((self.root/'safe.txt.upload').exists())

    def test_disk_readback_corruption_is_rejected(self):
        (self.root/'safe.txt').write_bytes(b'original')
        crc=zlib.crc32(b'abc')
        self.command(f"BEGIN {'safe.txt'.encode().hex()} 3 {crc:08x}")
        self.command(f'WRITE 0 616263 {crc:08x}')
        # Corrupt the file after a full stdio buffer has been flushed: use
        # an empty upload here so outside data remains visible to verification.
        self.command('ABORT')
        self.command(f"BEGIN {'safe.txt'.encode().hex()} 0 00000000")
        (self.root/'safe.txt.upload').write_bytes(b'corruption')
        self.assertIn('readback_failed',self.command('VERIFY'))
        self.assertEqual((self.root/'safe.txt').read_bytes(),b'original')

    def test_interrupted_replacement_restores_previous_on_next_boot(self):
        self.stop()
        (self.root/'Q650.ROM.previous').write_bytes(b'old ROM')
        (self.root/'.amac-update').write_text('Q650.ROM')
        self.start()
        self.assertEqual(self.control.sd_stat('Q650.ROM'),7)
        self.assertEqual((self.root/'Q650.ROM').read_bytes(),b'old ROM')
        self.assertFalse((self.root/'.amac-update').exists())

    def test_invalid_paths_lengths_numbers_and_empty_file(self):
        for name in ('../outside','x/y','x\\y','.hidden','x.upload','nul\x00','x.'):
            with self.subTest(name=name):
                self.assertIn('invalid_arguments',self.command(f'BEGIN {name.encode().hex()} 0 00000000'))
                with self.assertRaises(ControlError): self.control._sd_name(name)
        for bad in ('-1','2147483648','1x'):
            self.assertIn('invalid_arguments',self.command(f"BEGIN {'ok.txt'.encode().hex()} {bad} 00000000"))
        source=self.root/'empty';source.write_bytes(b'')
        self.control.sd_put(source,'empty.txt')
        output=self.root/'out';self.control.sd_get('empty.txt',output)
        self.assertEqual(output.read_bytes(),b'')
        self.assertIn('invalid_arguments',self.command(f"BEGIN {'old.previous'.encode().hex()} 0 00000000"))

if __name__=='__main__': unittest.main()
