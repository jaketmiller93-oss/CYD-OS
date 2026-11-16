"""Real packaged Lua apps, native bindings and LVGL at both panels/all rotations."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Essentials(unittest.TestCase):
 def test_packages_and_real_app_workflows(self):
  if not os.getenv('IDF_PATH'):self.skipTest('ESP-IDF required for native cJSON')
  spec=importlib.util.spec_from_file_location('suite',ROOT/'scripts/build_essentials.py');suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
  build=ROOT/'build-platform-host'
  subprocess.run(['cmake','-S',str(ROOT/'tests/platform_host'),'-B',str(build)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['cmake','--build',str(build),'--parallel','4'],check=True,stdout=subprocess.DEVNULL)
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);results=suite.build(p/'packages',p/'staging');self.assertEqual(len(results),6)
   preview=Path(os.getenv('CYD_ESSENTIALS_PREVIEW_DIR',str(p/'previews')));preview.mkdir(parents=True,exist_ok=True)
   for w,h in [(240,320),(320,240),(800,480),(480,800)]:
    for name in ['api',*suite.APPS]:
     with self.subTest(app=name,size=(w,h)):
      r=subprocess.run([str(build/'essentials_host'),str(p/'staging'/('notes' if name=='api' else name)),str(w),str(h),name,str(preview)],capture_output=True,text=True)
      self.assertEqual(r.returncode,0,r.stdout+r.stderr)
   # Real S3 layouts with 30 ms per native UI lock; ordinary drawing must not
   # consume the script's 500 ms allowance. Keep an absolute callback cap.
   for w,h in [(800,480),(480,800)]:
    for name in ['calculator','calendar','clock','notes','contacts']:
     with self.subTest(slow_ui=name,size=(w,h)):
      env=dict(os.environ,CYD_UI_DELAY_US='30000',CYD_ESSENTIALS_LAUNCH_ONLY='1')
      r=subprocess.run([str(build/'essentials_host'),str(p/'staging'/name),str(w),str(h),name,str(preview)],env=env,capture_output=True,text=True)
      self.assertEqual(r.returncode,0,r.stdout+r.stderr)
