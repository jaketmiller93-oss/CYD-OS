"""Actual credential implementation with a bounded, transactional NVS test double."""
import ctypes as C
import hashlib
import os
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class Credentials(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  idf=os.getenv('IDF_PATH')
  if not idf:raise unittest.SkipTest('Activate ESP-IDF for cJSON headers')
  cls.tmp=tempfile.TemporaryDirectory();out=Path(cls.tmp.name)/'credentials.so'
  # Uses actual package-ID/path validation, not a permissive test replacement.
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/system_sdk'),'-I'+str(ROOT/'components/app_services/include'),'-I'+str(Path(idf)/'components/json/cJSON'),str(ROOT/'tests/credentials_harness.c'),str(ROOT/'components/cyd_common/zip.c'),'-o',str(out)],check=True)
  cls.lib=C.CDLL(str(out));l=cls.lib;l.cyd_credentials_set.argtypes=[C.c_char_p,C.c_char_p,C.c_void_p,C.c_size_t];l.cyd_credentials_get.argtypes=[C.c_char_p,C.c_char_p,C.c_void_p,C.POINTER(C.c_size_t)];l.cyd_credentials_delete.argtypes=[C.c_char_p,C.c_char_p]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def setUp(self):self.lib.test_creds_reset()
 def put(self,app,key,value):return self.lib.cyd_credentials_set(app.encode(),key.encode(),value,len(value))
 def get(self,app,key):
  b=C.create_string_buffer(256);n=C.c_size_t(256);e=self.lib.cyd_credentials_get(app.encode(),key.encode(),b,C.byref(n));return e,b.raw[:n.value] if not e else None
 def test_application_ownership_and_binary_values(self):
  self.assertEqual(self.put('org.cyd.one','token',b'a\0b'),0);self.assertNotEqual(self.get('org.cyd.two','token')[0],0);self.assertEqual(self.put('org.cyd.two','token',b'second'),0);self.assertEqual(self.get('org.cyd.one','token'),(0,b'a\0b'));self.assertEqual(self.get('org.cyd.two','token'),(0,b'second'))
 def test_hash_collision_does_not_alias_full_identities(self):
  seen={};pair=None
  for i in range(200000):
   name=f'org.cyd.collision{i}';digest=hashlib.sha256(name.encode()).digest()[:4]
   if digest in seen:pair=(seen[digest],name);break
   seen[digest]=name
  self.assertIsNotNone(pair);a,b=pair;self.assertEqual(self.put(a,'key',b'first'),0);self.assertEqual(self.put(b,'key',b'second'),0);self.assertEqual(self.get(a,'key'),(0,b'first'));self.assertEqual(self.get(b,'key'),(0,b'second'))
 def test_quotas_delete_reuse_and_invalid_keys(self):
  for i in range(4):self.assertEqual(self.put('org.cyd.one',f'key{i}',b'x'),0)
  self.assertNotEqual(self.put('org.cyd.one','extra',b'x'),0);self.assertNotEqual(self.put('org.cyd.one','key0',b'x'*257),0);self.assertNotEqual(self.put('org.cyd.one','../token',b'x'),0)
  for i in range(1,8):self.assertEqual(self.put(f'org.cyd.app{i}','token',b'x'),0)
  self.assertNotEqual(self.put('org.cyd.ninth','token',b'x'),0);self.assertEqual(self.lib.cyd_credentials_delete(b'org.cyd.app1',b'token'),0);self.assertEqual(self.put('org.cyd.ninth','token',b'x'),0)
 def test_failed_commit_and_corrupt_record(self):
  self.put('org.cyd.one','key',b'old');self.lib.test_creds_fail(1);self.assertNotEqual(self.put('org.cyd.one','key',b'new'),0);self.assertEqual(self.get('org.cyd.one','key'),(0,b'old'));self.lib.test_creds_fail(0);self.lib.test_creds_corrupt();self.assertNotEqual(self.get('org.cyd.one','key')[0],0)
