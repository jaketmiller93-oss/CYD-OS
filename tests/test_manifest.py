"""Compile the firmware manifest validator against ESP-IDF's real cJSON."""
import ctypes
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]

class NativeManifest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        idf=os.getenv('IDF_PATH')
        if not idf: raise unittest.SkipTest('Activate ESP-IDF to run native manifest tests')
        cjson=Path(idf)/'components/json/cJSON'
        if not (cjson/'cJSON.c').is_file(): raise unittest.SkipTest('ESP-IDF cJSON source unavailable')
        cls.tmp=tempfile.TemporaryDirectory();cls.libs={}
        for target in ['esp32s3','esp32c5']:
            output=Path(cls.tmp.name)/(target+'.so')
            subprocess.run(['cc','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all',
                '-DCYD_HOST_TEST','-DCONFIG_IDF_TARGET="'+target+'"',
                '-I'+str(ROOT/'tests/host'),'-I'+str(ROOT/'tests'),'-I'+str(cjson),
                str(ROOT/'components/cyd_common/zip.c'),str(ROOT/'components/app_manager/manifest.c'),
                str(cjson/'cJSON.c'),'-lm','-o',str(output)],check=True)
            lib=ctypes.CDLL(str(output));lib.cyd_manifest_parse.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p];cls.libs[target]=lib
    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()
    def parse(self,data,target='esp32s3'):
        return self.libs[target].cyd_manifest_parse(data,len(data),ctypes.create_string_buffer(512))
    def sample(self,**changes):
        m=json.loads((ROOT/'apps/hello/manifest.json').read_text());m.update(changes);return json.dumps(m).encode()
    def test_both_targets_accept_shared_manifest(self):
        for target in self.libs:self.assertEqual(self.parse(self.sample(),target),0)
    def test_target_is_enforced(self):
        self.assertNotEqual(self.parse(self.sample(target=['esp32s3']),'esp32c5'),0)
        self.assertNotEqual(self.parse(self.sample(target=['esp32c5']),'esp32s3'),0)
    def test_deep_json_rejected_without_stack_exhaustion(self):
        self.assertNotEqual(self.parse(b'['*1000+b'0'+b']'*1000),0)
    def test_nul_duplicate_and_trailing_garbage_rejected(self):
        valid=self.sample()
        for raw in [valid+b'\x00extra',valid+b' extra',valid[:-1]+b',"name":"duplicate"}',self.sample(name='a\x00b')]:
            self.assertNotEqual(self.parse(raw),0)
    def test_permission_and_schema_failures(self):
        for changes in [dict(api_version=2),dict(permissions=['gpio']),dict(entry_point='../main.lua'),dict(icon=4),dict(name=''),dict(version='\n')]:
            self.assertNotEqual(self.parse(self.sample(**changes)),0)

if __name__=='__main__':unittest.main()
