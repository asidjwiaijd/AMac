"""Exercise the production boot Wi-Fi initialization and scan against a failing radio."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class WifiStartupTests(unittest.TestCase):
    def test_transport_failure_and_scan_retry(self):
        source=(ROOT/'src/basilisk/boot_gui.cpp').read_text()
        function='static bool initWiFi(void)\n{'+source.split('static bool initWiFi(void)\n{',1)[1].split('\n}\n',1)[0]+'\n}\n'
        scan=source.split('    auto startScan = [&]() {',1)[1].split('    startScan();',1)[0]
        scan='auto startScan = [&]() {'+scan
        fixture=(ROOT/'test/test_wifi_startup.cpp').read_text().replace('// INSERT_INIT',function).replace('// INSERT_SCAN',scan)
        compiler=os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.environ.get('AMAC_SANITIZERS') else []
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'wifi.cpp';cpp.write_text(fixture)
            binary=Path(temp)/'wifi'
            subprocess.run([compiler,'-std=c++17','-O1','-g',*flags,str(cpp),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=15)

if __name__=='__main__':unittest.main()
