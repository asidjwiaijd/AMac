"""Run the production USB host with asynchronous transfers under sanitizers."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]

class UsbHidHostTests(unittest.TestCase):
    def test_continuous_receive_and_composite_interfaces(self):
        compiler=os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.environ.get('AMAC_SANITIZERS') else []
        with tempfile.TemporaryDirectory() as temp:
            binary=Path(temp)/'usb-host'
            subprocess.run([compiler,'-std=c++17','-O1','-g',*flags,'-I',str(ROOT/'src/basilisk'),
                '-I',str(ROOT/'src/basilisk/include'),str(ROOT/'test/test_usb_hid_host.cpp'),
                str(ROOT/'src/basilisk/hid_descriptor.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=20)

if __name__=='__main__':unittest.main()
