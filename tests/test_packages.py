import ctypes
import io
import json
from pathlib import Path
import random
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/cydctl'))
import cydctl

def archive(files, compression=zipfile.ZIP_STORED):
    buf=io.BytesIO()
    with zipfile.ZipFile(buf,'w',compression=compression) as z:
        for name,data in files:
            i=zipfile.ZipInfo(name);i.external_attr=(stat.S_IFREG|0o644)<<16;i.compress_type=compression
            z.writestr(i,data)
    return buf.getvalue()

class Packages(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory()
        lib=Path(cls.tmp.name)/'zip.so'
        subprocess.run(['cc','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-DCYD_HOST_TEST','-I'+str(ROOT/'tests'),str(ROOT/'components/cyd_common/zip.c'),'-o',str(lib)],check=True)
        cls.lib=ctypes.CDLL(str(lib))
        cls.lib.cyd_zip_parse.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p]
    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()
    def parse(self,data):
        # Generous aligned output buffer, with exact input size checked by the C parser.
        out=ctypes.create_string_buffer(8192)
        return self.lib.cyd_zip_parse(data,len(data),out)
    def test_real_package_accepted(self):
        manifest,data=cydctl.package_project(ROOT/'apps/hello')
        self.assertEqual(self.parse(data),0)
        self.assertEqual(manifest['package_id'],'com.local.hello')
    def test_deterministic_package(self):
        self.assertEqual(cydctl.package_project(ROOT/'apps/hello'),cydctl.package_project(ROOT/'apps/hello'))
    def test_traversal_and_aliases(self):
        for name in ['../main.lua','/main.lua','a/../../x','a\\main.lua','A.lua','a//b','a/./b','a./b','a/../b','a:main.lua','a/b/c/d/e/f.lua']:
            with self.subTest(name=name): self.assertNotEqual(self.parse(archive([(name,b'x')])),0)
    def test_embedded_nul(self):
        data=archive([('abc.lua',b'x')]).replace(b'abc.lua',b'a\x00c.lua')
        self.assertNotEqual(self.parse(data),0)
    def test_duplicate_and_file_directory_collision(self):
        for files in [[('main.lua',b'a'),('main.lua',b'b')],[('a',b'a'),('a/b',b'b')],[('a/b',b'a'),('a',b'b')]]:
            self.assertNotEqual(self.parse(archive(files)),0)
    def test_crc_mismatch(self):
        data=bytearray(archive([('main.lua',b'payload')]))
        data[data.index(b'payload')]=ord('z')
        self.assertNotEqual(self.parse(bytes(data)),0)
    def test_reject_compression(self): self.assertNotEqual(self.parse(archive([('main.lua',b'hello'*50)],zipfile.ZIP_DEFLATED)),0)
    def test_symlink_rejected(self):
        data=bytearray(archive([('main.lua',b'x')]))
        cd=data.index(b'PK\x01\x02');struct.pack_into('<I',data,cd+38,(stat.S_IFLNK|0o777)<<16)
        self.assertNotEqual(self.parse(bytes(data)),0)
    def test_encryption_and_local_central_mismatch(self):
        for offset,value in [(6,1),(8,8),(18,200),(26,1)]:
            data=bytearray(archive([('main.lua',b'x')]))
            struct.pack_into('<H',data,offset,value)
            self.assertNotEqual(self.parse(bytes(data)),0)
    def test_archive_and_file_limits(self):
        self.assertNotEqual(self.parse(archive([('x.lua',b'x'*65537)])),0)
        self.assertNotEqual(self.parse(archive([(f'{i}.lua',b'x') for i in range(49)])),0)
    def test_truncations_and_random_mutations_do_not_crash(self):
        data=archive([('manifest.json',b'{}'),('main.lua',b'print(1)')])
        for n in range(len(data)): self.assertNotEqual(self.parse(data[:n]),0)
        rng=random.Random(7)
        for _ in range(2500):
            b=bytearray(data)
            for _ in range(rng.randrange(1,7)):b[rng.randrange(len(b))]=rng.randrange(256)
            self.parse(bytes(b))
    def test_manifest_permissions_and_targets(self):
        m=json.loads((ROOT/'apps/hello/manifest.json').read_text())
        for key,value in [('permissions',['gpio']),('api_version',2),('target',[]),('entry_point','../main.lua')]:
            changed=dict(m);changed[key]=value
            with self.assertRaises(cydctl.CydError):cydctl.validate_manifest(json.dumps(changed).encode())
    def test_duplicate_manifest_key(self):
        with self.assertRaises(cydctl.CydError):cydctl.validate_manifest(b'{"name":"one","name":"two"}')
    def test_client_rejects_symlink(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp)/'main.lua').symlink_to(ROOT/'apps/hello/main.lua')
            with self.assertRaises(cydctl.CydError):cydctl.package_project(Path(tmp))

if __name__=='__main__': unittest.main()
