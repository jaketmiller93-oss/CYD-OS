"""Build and execute actual runtime bindings with Lua/LVGL, ASan and UBSan."""
import os,shutil,subprocess,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class LuaPlatform(unittest.TestCase):
 def test_real_lua_lvgl_runtime(self):
  if not os.getenv('IDF_PATH'):self.skipTest('Activate ESP-IDF for cJSON')
  if not (ROOT/'managed_components/lvgl__lvgl/CMakeLists.txt').is_file():self.skipTest('Fetch pinned LVGL with an IDF configure/build first')
  cmake=shutil.which('cmake')
  if not cmake:self.skipTest('Install CMake for native Lua/LVGL tests')
  build=ROOT/'build-platform-host'
  subprocess.run([cmake,'-S',str(ROOT/'tests/platform_host'),'-B',str(build)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run([cmake,'--build',str(build),'--parallel','4'],check=True,stdout=subprocess.DEVNULL)
  for args in [[],[str(ROOT/'apps/platform_demo'),'320'],[str(ROOT/'apps/platform_demo'),'800']]:
   result=subprocess.run([str(build/'platform_host')]+args,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
