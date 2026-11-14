"""Actual shared firmware loader/preflight plus host package inspection agreement."""
import ctypes as C,json,os,subprocess,tempfile,unittest,zipfile,sys,random
from pathlib import Path
from test_packages import archive,ROOT
sys.path.insert(0,str(ROOT/'tools/cydctl'))
import cydctl
class Preview(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  idf=os.getenv('IDF_PATH')
  if not idf:raise unittest.SkipTest('Activate ESP-IDF for cJSON')
  cls.tmp=tempfile.TemporaryDirectory();out=Path(cls.tmp.name)/'preview.so';cj=Path(idf)/'components/json/cJSON'
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-DCYD_HOST_TEST','-DCONFIG_IDF_TARGET="esp32c5"','-I'+str(ROOT/'tests/host'),'-I'+str(ROOT/'tests'),'-I'+str(cj),str(ROOT/'tests/package_preview_harness.c'),str(ROOT/'components/cyd_common/zip.c'),str(ROOT/'components/cyd_common/json.c'),str(ROOT/'components/app_manager/manifest.c'),str(ROOT/'components/app_manager/package.c'),str(cj/'cJSON.c'),'-lm','-o',str(out)],check=True)
  cls.lib=C.CDLL(str(out));cls.lib.test_package_preview.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def preview(self,data):
  out=C.create_string_buffer(8192);rc=self.lib.test_package_preview(data,len(data),out,len(out));return rc,json.loads(out.value) if not rc else out.value.decode()
 def fixture(self,**changes):
  m=json.loads((ROOT/'apps/hello/manifest.json').read_text());m.update(changes);return archive([('manifest.json',json.dumps(m).encode()),('main.lua',b'print("hello")')])
 def test_new_permissions_and_existing_api_one_apps(self):
  for permissions in [[],['storage'],['network','mqtt','credentials'],['storage','network','mqtt','credentials']]:
   raw=self.fixture(permissions=permissions);rc,m=self.preview(raw);self.assertEqual(rc,0);self.assertEqual(m['permissions'],permissions);self.assertEqual(cydctl.inspect_package(raw)['permissions'],permissions)
 def test_rejects_unsupported_permissions_api_and_missing_entry(self):
  for change in [dict(permissions=['gpio']),dict(api_version=2),dict(entry_point='missing.lua'),dict(target=['esp32s3'])]:self.assertNotEqual(self.preview(self.fixture(**change))[0],0)
 def test_host_and_firmware_inspect_reject_same_zip_mutations(self):
  raw=self.fixture(permissions=['network']);rng=random.Random(402)
  for _ in range(300):
   bad=bytearray(raw);bad[rng.randrange(len(bad))]^=rng.randrange(1,256);fw=self.preview(bytes(bad))[0]==0
   try:cydctl.inspect_package(bytes(bad));host=True
   except cydctl.CydError:host=False
   self.assertEqual(host,fw)
 def test_packaged_demo_and_cli_inspect(self):
  m,raw=cydctl.package_project(ROOT/'apps/platform_demo');self.assertEqual(self.preview(raw)[0],0);self.assertEqual(cydctl.inspect_package(raw)['package_id'],m['package_id'])
  out=subprocess.run([sys.executable,str(ROOT/'tools/cydctl/cydctl.py'),'inspect',str(ROOT/'apps/platform_demo'),'--json'],capture_output=True,text=True,check=True);self.assertEqual(json.loads(out.stdout)['permissions'],['network','mqtt','credentials'])
