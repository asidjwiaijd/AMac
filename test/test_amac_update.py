"""Refuse stale, truncated or uncommitted firmware before a device is opened."""
from pathlib import Path
import hashlib
import json
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from amac_update import load_application


class UpdateImageTests(unittest.TestCase):
    def test_guard_release_hash_application_identity_and_source(self):
        with tempfile.TemporaryDirectory() as folder:
            image=Path(folder)/'firmware.bin'
            application=bytearray(128);application[0]=0xe9;application[12]=18
            merged=bytes(0x10000)+application
            metadata={'merged_sha256':hashlib.sha256(merged).hexdigest(),
                      'components':{'firmware.bin':hashlib.sha256(application).hexdigest()},'source_dirty':False}
            image.write_bytes(merged)
            def save(): image.with_suffix('.json').write_text(json.dumps(metadata))
            save();self.assertEqual(load_application(image)[2],application)
            metadata['source_dirty']=True;save()
            with self.assertRaises(ValueError): load_application(image)
            metadata['source_dirty']=False;metadata['components']['firmware.bin']='0'*64;save()
            with self.assertRaises(ValueError): load_application(image)
            image.write_bytes(merged[:-1])
            with self.assertRaises(ValueError): load_application(image)
            bad=bytearray(merged);bad[0x10000+12]=9;image.write_bytes(bad)
            metadata['merged_sha256']=hashlib.sha256(bad).hexdigest();save()
            with self.assertRaises(ValueError): load_application(image)

if __name__=='__main__': unittest.main()
