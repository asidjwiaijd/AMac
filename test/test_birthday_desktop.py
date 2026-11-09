"""Validate native Finder packages and the actual SDL desktop integration."""
import importlib.util
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("birthday_package",ROOT/"guest/birthday/package.py")
package=importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

def parse_resource_fork(raw):
    data_start,map_start,data_size,map_size=struct.unpack_from(">IIII",raw)
    assert map_start+map_size==len(raw) and data_start+data_size==map_start
    assert raw[map_start:map_start+16]==raw[:16]
    types=map_start+struct.unpack_from(">H",raw,map_start+24)[0]
    count=struct.unpack_from(">H",raw,types)[0]+1
    resources={}
    for i in range(count):
        at=types+2+i*8
        kind=raw[at:at+4]
        n,refs=struct.unpack_from(">HH",raw,at+4)
        for j in range(n+1):
            entry=types+refs+j*12
            rid=struct.unpack_from(">h",raw,entry)[0]
            offset=int.from_bytes(raw[entry+5:entry+8],"big")
            location=data_start+offset
            size=struct.unpack_from(">I",raw,location)[0]
            assert location+4+size<=map_start
            resources[kind,rid]=raw[location+4:location+4+size]
    return resources

class BirthdayPackageTests(unittest.TestCase):
    def test_native_code_entry_and_resource_maps(self):
        for name,creator,entry in package.APPS:
            with self.subTest(app=name):
                expected=package.application(name,creator,entry)
                target=ROOT/"guest/birthday/dist"/f"{name}.bin"
                self.assertEqual(target.read_bytes(),expected)
                self.assertEqual(package.crc16(expected[:124]),struct.unpack_from(">H",expected,124)[0])
                identity,data,raw=package.read_macbinary(target)
                self.assertEqual(identity,b"APPL"+creator.encode()); self.assertFalse(data)
                resources=parse_resource_fork(raw)
                code0,code1=resources[b"CODE",0],resources[b"CODE",1]
                above,below,length,offset=struct.unpack_from(">IIII",code0)
                self.assertEqual((above,below,length,offset),(40,256,8,32))
                routine,push,segment,trap=struct.unpack_from(">HHHH",code0,16)
                self.assertEqual((push,segment,trap),(0x3f3c,1,0xa9f0))
                self.assertEqual(struct.unpack_from(">HH",code1),(0,1))
                # Near-model loader adds the segment's 4-byte header first.
                self.assertEqual(code1[4+routine:6+routine],b"\x59\x8f")
                self.assertEqual(struct.unpack_from(">9H",code1,4),
                                 (0x598f,0x2f3c,0x6244,0x4159,0x3f3c,128,0xa9a0,0x205f,0xa9f4))
                self.assertEqual(resources[b"bDAY",128],b"aMacBday\x01"+bytes([entry]))
                self.assertEqual(len(resources[b"ICN#",128]),256)
                self.assertEqual(resources[b"ICN#",128],resources[b"ICN#",-16455])

    def test_installer_preserves_unrelated_content(self):
        with tempfile.TemporaryDirectory() as folder:
            old,new=(Path(folder)/n for n in ("old.bin","new.bin"))
            new.write_bytes(package.application("For You","bGft",1))
            old.write_bytes(new.read_bytes()); self.assertEqual(package.compare_for_update(old,new),0)
            damaged=bytearray(new.read_bytes()); index=damaged.index(b"aMacBday")
            damaged[index-3]^=1; old.write_bytes(damaged)
            self.assertEqual(package.compare_for_update(old,new),1)
            old.write_bytes(package.macbinary("For You",b"TEXT",b"ttxt",b"user note"))
            with self.assertRaises(ValueError): package.compare_for_update(old,new)

class BirthdayDesktopTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("pkg-config"),"SDL2 host test requires pkg-config")
    def test_actual_sdl_adapter(self):
        flags=subprocess.check_output(["pkg-config","--cflags","--libs","sdl2"],text=True).split()
        compiler=os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
        with tempfile.TemporaryDirectory() as folder:
            binary=Path(folder)/"birthday-desktop-test"
            command=[compiler,"-std=c++17","-O1","-g","-pthread","-DBIRTHDAY_HOST_TEST=1",
                     "-fsanitize=address,undefined","-fno-omit-frame-pointer"]
            for include in ("examples/amac-desktop","src/basilisk/include","src/generated"):
                command += ["-I",str(ROOT/include)]
            command += [str(ROOT/p) for p in ("test/test_birthday_desktop.cpp",
                "examples/amac-desktop/birthday_bridge.cpp","src/basilisk/birthday_gift.cpp",
                "src/basilisk/birthday_gift_art.cpp")]
            subprocess.run(command+flags+["-o",str(binary)],check=True)
            subprocess.run([str(binary)],check=True,env=dict(os.environ,SDL_VIDEODRIVER="dummy"),timeout=30)

if __name__=="__main__": unittest.main()
