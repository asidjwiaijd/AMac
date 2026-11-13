"""ESP image integrity and exact-version guards for the P4 startup wait change."""
from pathlib import Path
import hashlib
import struct
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'scripts'),str(ROOT/'tools')]
import patch_amac_bootloader as patcher
from amac_update import load_bootloader,bootloader_migration


def fixture():
    header=bytearray(24);header[0]=0xe9;header[1]=1;header[12]=18;header[23]=1
    payload=bytearray(32);payload[14:18]=bytes.fromhex('1305803e')
    image=header+struct.pack('<II',0x4fface00,len(payload))+payload
    checksum=0xef
    for value in payload:checksum^=value
    image+=bytes(15)+bytes([checksum])
    return bytes(image+hashlib.sha256(image).digest())


class BootloaderWaitTests(unittest.TestCase):
    def test_delay_only_change_and_integrity(self):
        original=fixture()
        changed=patcher._rewrite_delay(original,2000)
        self.assertEqual(changed[46:50],bytes.fromhex('1305007d'))
        self.assertEqual(changed[:46],original[:46])
        self.assertEqual(changed[50:79],original[50:79])
        self.assertEqual(changed[79],original[79]^0xc3) # Independent XOR of the two changed instruction bytes.
        patcher.image_layout(changed)
        self.assertEqual(patcher._rewrite_delay(changed,1000),original)
        self.assertEqual(patcher._rewrite_delay(changed,2000),changed)

    def test_reject_unknown_or_damaged_images(self):
        original=fixture()
        with self.assertRaises(ValueError):patcher.patch_bootloader(original)
        with self.assertRaises(ValueError):patcher.paired_bootloader(original)
        for index in (0,12,23,40,79,90):
            damaged=bytearray(original);damaged[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):patcher._rewrite_delay(damaged,2000)
        for truncated in (original[:20],original[:30],original[:-1]):
            with self.assertRaises(ValueError):patcher._rewrite_delay(truncated,2000)

    def test_merged_bootloader_bounds_and_metadata(self):
        boot=fixture();merged=bytes(0x2000)+boot+bytes(0x10000-0x2000-len(boot))
        metadata={'components':{'bootloader.bin':hashlib.sha256(boot).hexdigest()}}
        self.assertEqual(load_bootloader(metadata,merged),boot)
        metadata['components']['bootloader.bin']='0'*64
        with self.assertRaises(ValueError):load_bootloader(metadata,merged)
        with self.assertRaises(ValueError):load_bootloader(metadata,merged[:0x2020])
        self.assertIsNone(bootloader_migration(boot,hashlib.md5(boot).hexdigest()))
        with self.assertRaises(ValueError):bootloader_migration(boot,'0'*32)

    def test_installed_pinned_bootloader_roundtrip(self):
        path=ROOT/'.pio/build/amac_p4_rev3/bootloader.bin'
        if not path.exists():self.skipTest('Build the pinned SDK first for binary integration')
        image=path.read_bytes()
        result=patcher.patch_bootloader(image)
        self.assertEqual(hashlib.sha256(result).hexdigest(),patcher.PATCHED_SHA256)
        original=patcher.paired_bootloader(result)
        self.assertEqual(hashlib.sha256(original).hexdigest(),patcher.ORIGINAL_SHA256)
        self.assertEqual(bootloader_migration(result,hashlib.md5(original).hexdigest()),original)
        self.assertEqual(bootloader_migration(original,hashlib.md5(result).hexdigest()),result)
        with self.assertRaises(ValueError):bootloader_migration(result,'0'*32)
