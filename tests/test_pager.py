"""Real LVGL pager: bounded controls, no drag scrolling, long text and ownership."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class Pager(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if not os.getenv('IDF_PATH'):raise unittest.SkipTest('Activate ESP-IDF for native LVGL build')
  build=ROOT/'build-platform-host'
  subprocess.run(['cmake','-S',str(ROOT/'tests/platform_host'),'-B',str(build)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['cmake','--build',str(build),'--parallel','4'],check=True,stdout=subprocess.DEVNULL)
  cls.tmp=tempfile.TemporaryDirectory();cls.binary=Path(cls.tmp.name)/'pager'
  subprocess.run(['cc','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DLV_CONF_PATH='+str(ROOT/'tests/ui_preview/lv_conf.h'),'-I'+str(ROOT/'components/ui'),'-I'+str(ROOT/'managed_components/lvgl__lvgl'),str(ROOT/'tests/pager_host.c'),str(ROOT/'components/ui/pager.c'),str(build/'lib/liblvgl.a'),'-lm','-o',str(cls.binary)],check=True)
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def case(self,n):
  r=subprocess.run([str(self.binary),str(n)],capture_output=True,text=True);self.assertEqual(r.returncode,0,r.stdout+r.stderr)
 def test_column_boundaries_and_pointer_targets(self):self.case(0)
 def test_grid_has_complete_icon_rows(self):self.case(1)
 def test_long_text_live_update_resize_and_reset(self):self.case(2)
 def test_repeated_page_deletion_frees_state(self):self.case(3)
