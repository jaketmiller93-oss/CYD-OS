"""Actual role-specific LVGL shell and file controls with service IO doubles."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class PairedUI(unittest.TestCase):
 def test_roles_files_and_management_routes(self):
  idf=os.getenv('IDF_PATH')
  if not idf:self.skipTest('ESP-IDF cJSON required')
  build=ROOT/'build-platform-host';cj=Path(idf)/'components/json/cJSON'
  subprocess.run(['cmake','-S',str(ROOT/'tests/platform_host'),'-B',str(build)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['cmake','--build',str(build),'--parallel','4'],check=True,stdout=subprocess.DEVNULL)
  cmd=['cc','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DCYD_UI_PREVIEW','-DCYD_UI_MEMORY_TEST','-DCYD_PAIRED_UI_TEST','-DCONFIG_CYD_PAIRED_SYSTEM=1','-DLV_CONF_PATH='+str(ROOT/'tests/ui_preview/lv_conf.h')]
  for inc in [ROOT/'tests/system_sdk',ROOT/'tests/pair_crypto',cj,ROOT/'managed_components/lvgl__lvgl',ROOT/'components/ui']+[ROOT/'components'/c/'include' for c in ['network','device_settings','device_access','ota_manager','app_manager','device_link','paired_system','builtin_apps']]:cmd+=['-I'+str(inc)]
  src=[ROOT/'tests/paired_ui_host.c',ROOT/'tests/system_runtime.c',ROOT/'tests/ui_preview/watch_mocks.c',cj/'cJSON.c',ROOT/'components/device_settings/geometry.c',ROOT/'components/network/policy.c',ROOT/'components/cyd_common/json.c']+[ROOT/'components/ui'/s for s in ['ui.c','settings.c','connections.c','projects.c','pager.c','pair_ui.c']]
  with tempfile.TemporaryDirectory() as tmp:
   for target in ['c5','s3']:
    out=Path(tmp)/target
    subprocess.run(cmd+(['-DCONFIG_IDF_TARGET_ESP32S3=1']if target=='s3'else[])+[str(p)for p in src]+[str(build/'lib/liblvgl.a'),'-lm','-lcrypto','-o',str(out)],check=True)
    r=subprocess.run([str(out)],capture_output=True,text=True);self.assertEqual(r.returncode,0,target+': '+r.stdout+r.stderr);print(target,r.stdout)
