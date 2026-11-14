import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('connection_update', ROOT/'scripts/apply_connection_update.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class Update(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.base=Path(self.tmp.name);self.project=self.base/'cyd-os';self.bundle=self.base/'update';self.project.mkdir();self.bundle.mkdir()
  for name,data in {'CMakeLists.txt':'project(cyd-os)','components/network/network.c':'old network','build-device-c5/sdkconfig':'PRIVATE CONFIG','apps/hello/main.lua':'custom greeting','boards/nm_cyd_c5/board.c':'calibrated touch'}.items():
   p=self.project/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data)
  self.files={}
  for name,data in {'components/network/network.c':b'new network','docs/CONNECTIONS.md':b'new guide'}.items():
   p=self.bundle/'payload'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);self.files[name]=hashlib.sha256(data).hexdigest()
  self.manifest()
 def tearDown(self):self.tmp.cleanup()
 def manifest(self): (self.bundle/'manifest.json').write_text(json.dumps({'format':1,'files':self.files}))
 def test_update_preserves_config_apps_calibration_and_backs_up_sources(self):
  backup,count=module.apply_update(self.bundle,self.project)
  self.assertEqual(count,2);self.assertEqual((backup/'components/network/network.c').read_text(),'old network')
  for name,data in [('build-device-c5/sdkconfig','PRIVATE CONFIG'),('apps/hello/main.lua','custom greeting'),('boards/nm_cyd_c5/board.c','calibrated touch')]:self.assertEqual((self.project/name).read_text(),data)
  self.assertEqual((self.project/'components/network/network.c').read_text(),'new network')
 def test_bad_digest_is_rejected_before_replacing_any_file(self):
  self.files['docs/CONNECTIONS.md']='bad';self.manifest()
  with self.assertRaises(ValueError):module.apply_update(self.bundle,self.project)
  self.assertEqual((self.project/'components/network/network.c').read_text(),'old network')
  self.assertFalse(list(self.project.glob('.connection-manager-backup-*')))
 def test_generated_configs_and_application_paths_are_rejected(self):
  for path in ['build-device-c5/sdkconfig','apps/hello/main.lua','../outside','components/../../outside','sdkconfig']:
   self.files={path:'x'};self.manifest()
   with self.assertRaises(ValueError):module.apply_update(self.bundle,self.project)
 def test_symlink_target_is_rejected(self):
  external=self.base/'outside';external.write_text('outside');target=self.project/'components/network/network.c';target.unlink();target.symlink_to(external)
  with self.assertRaises(ValueError):module.apply_update(self.bundle,self.project)
  self.assertEqual(external.read_text(),'outside')

if __name__=='__main__':unittest.main()
