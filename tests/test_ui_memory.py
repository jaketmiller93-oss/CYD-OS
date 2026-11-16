"""Real UI startup under an internal buffer-allocation limit, plus failed PSRAM cleanup."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class UIMemory(unittest.TestCase):
 def test_shell_buffers_and_failure_cleanup_on_both_boards(self):
  idf=os.getenv('IDF_PATH')
  if not idf:self.skipTest('Activate ESP-IDF for cJSON')
  build=ROOT/'build-platform-host'
  subprocess.run(['cmake','-S',str(ROOT/'tests/platform_host'),'-B',str(build)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['cmake','--build',str(build),'--parallel','4'],check=True,stdout=subprocess.DEVNULL)
  cj=Path(idf)/'components/json/cJSON'
  with tempfile.TemporaryDirectory() as tmp:
   command=['cc','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DCYD_UI_PREVIEW','-DCYD_UI_MEMORY_TEST','-DLV_CONF_PATH='+str(ROOT/'tests/ui_preview/lv_conf.h')]
   for inc in [ROOT/'tests/system_sdk',cj,ROOT/'managed_components/lvgl__lvgl',ROOT/'components/ui']+[ROOT/'components'/c/'include' for c in ['network','device_settings','device_access','ota_manager','app_manager','device_link']]:command+=['-I'+str(inc)]
   sources=[ROOT/'tests/ui_memory_host.c',ROOT/'tests/system_runtime.c',ROOT/'tests/ui_preview/watch_mocks.c',cj/'cJSON.c',ROOT/'components/device_settings/geometry.c',ROOT/'components/network/policy.c']+[ROOT/'components/ui'/s for s in ['ui.c','settings.c','connections.c','projects.c','pager.c']]
   for target in ['c5','s3']:
    binary=Path(tmp)/target
    subprocess.run(command+(['-DCONFIG_IDF_TARGET_ESP32S3'] if target=='s3' else [])+[str(p) for p in sources]+[str(build/'lib/liblvgl.a'),'-lm','-o',str(binary)],check=True)
    for failure in [0,1,2,3]:
     r=subprocess.run([str(binary),str(failure)],capture_output=True,text=True)
     self.assertEqual(r.returncode,0,target+': '+r.stdout+r.stderr)
     print(target, r.stdout.strip())
