"""Settings UI exercises the real settings worker and LVGL with board SDK doubles."""
from pathlib import Path
import os,subprocess,tempfile,unittest,shutil
ROOT=Path(__file__).resolve().parents[1]
class SettingsUI(unittest.TestCase):
 def test_preferences_diagnostics_and_navigation(self):
  idf=os.getenv('IDF_PATH')
  if not idf:self.skipTest('Activate ESP-IDF for cJSON headers')
  build=ROOT/'build-platform-host'
  subprocess.run(['cmake','-S',str(ROOT/'tests/platform_host'),'-B',str(build)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['cmake','--build',str(build),'--parallel','4'],check=True,stdout=subprocess.DEVNULL)
  with tempfile.TemporaryDirectory() as tmp:
   out=Path(tmp);binary=out/'settings_ui';command=['cc','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DCYD_UI_PREVIEW','-DLV_CONF_PATH='+str(ROOT/'tests/ui_preview/lv_conf.h')]
   for inc in [ROOT/'tests/system_sdk',ROOT/'components/ui',ROOT/'components/device_link/include',ROOT/'components/device_settings/include',ROOT/'managed_components/lvgl__lvgl',Path(idf)/'components/json/cJSON']:command+=['-I'+str(inc)]
   subprocess.run(command+[str(ROOT/'tests/settings_ui_host.c'),str(ROOT/'tests/system_runtime.c'),str(ROOT/'components/device_settings/geometry.c'),str(ROOT/'components/ui/pager.c'),str(build/'lib/liblvgl.a'),'-lm','-o',str(binary)],check=True)
   for w,h in [(240,320),(320,240),(800,480),(480,800)]:
    result=subprocess.run([str(binary),str(w),str(h),str(out)],capture_output=True,text=True)
    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    print(result.stdout.strip())
   destination=os.getenv('CYD_SETTINGS_PREVIEW_DIR')
   if destination:
    dest=Path(destination);dest.mkdir(parents=True,exist_ok=True)
    for p in out.glob('*.ppm'):shutil.copy2(p,dest/p.name)
