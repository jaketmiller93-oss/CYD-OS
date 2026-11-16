"""Actual LVGL Apps grid: exercise point input and folder/version operations with SDK doubles."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ProjectsUI(unittest.TestCase):
 def test_icon_browser_interactions_and_layouts(self):
  idf=os.getenv('IDF_PATH')
  if not idf:self.skipTest('Activate ESP-IDF for cJSON')
  build=ROOT/'build-platform-host'
  if not (build/'lib/liblvgl.a').exists():
   subprocess.run(['cmake','-S',str(ROOT/'tests/platform_host'),'-B',str(build)],check=True,stdout=subprocess.DEVNULL)
   subprocess.run(['cmake','--build',str(build),'--parallel','4'],check=True,stdout=subprocess.DEVNULL)
  with tempfile.TemporaryDirectory() as tmp:
   out=Path(tmp);binary=out/'projects_ui';cj=Path(idf)/'components/json/cJSON'
   command=['cc','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DCYD_UI_PREVIEW','-DLV_CONF_PATH='+str(ROOT/'tests/ui_preview/lv_conf.h')]
   for inc in [ROOT/'tests/system_sdk',ROOT/'components/ui',ROOT/'components/app_manager/include',ROOT/'managed_components/lvgl__lvgl',cj]:command+=['-I'+str(inc)]
   subprocess.run(command+[str(ROOT/'tests/projects_ui_host.c'),str(cj/'cJSON.c'),str(ROOT/'components/ui/pager.c'),str(build/'lib/liblvgl.a'),'-lm','-o',str(binary)],check=True)
   for w,h in [(320,240),(240,320),(800,480),(480,800)]:
    result=subprocess.run([str(binary),str(w),str(h),str(out)],capture_output=True,text=True)
    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    # Optional visual artifacts from exactly this tested renderer, outside test temp files.
    destination=os.getenv('CYD_PROJECTS_PREVIEW_DIR')
    if destination:
     import shutil
     dest=Path(destination);dest.mkdir(parents=True,exist_ok=True)
     for image in out.glob(f'*-{w}x{h}.ppm'):shutil.copy2(image,dest/image.name)
    print(result.stdout.strip())
