"""The one-time updater must preserve credentials, device calibration and apps."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from apply_os_update import bootstrap
from ota_config import configured_text
class OSUpdate(unittest.TestCase):
 def fixture(self,directory):
  root=Path(directory);project=root/'project';bundle=root/'bundle';(project/'components/network').mkdir(parents=True);(bundle/'payload/components/ui').mkdir(parents=True)
  (project/'CMakeLists.txt').write_text('project(cyd-os)');(project/'components/network/network.c').write_text('old network');(bundle/'payload/components/ui/settings.c').write_text('new settings')
  (bundle/'manifest.json').write_text(json.dumps({'format':1,'files':{'components/ui/settings.c':hashlib.sha256(b'new settings').hexdigest()}}))
  cfg=project/'build-device-c5/sdkconfig';cfg.parent.mkdir();cfg.write_text('CONFIG_IDF_TARGET_ESP32C5=y\nCONFIG_CYD_BOARD_CONFIRMED=y\nCONFIG_CYD_WIFI_PASSWORD="keep-password"\nCONFIG_CYD_API_TOKEN="keep-device-token"\nCONFIG_CYD_C5_TOUCH_X_MIN=333\n# CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE is not set\nCONFIG_BOOTLOADER_LOG_LEVEL_INFO=y\n')
  return project,bundle,cfg
 def test_bootstrap_changes_flags_preserves_private_config_and_backup(self):
  with tempfile.TemporaryDirectory() as t:
   project,bundle,cfg=self.fixture(t);original=cfg.read_text();backup,count=bootstrap(bundle,project)
   self.assertEqual(count,1);text=cfg.read_text()
   for line in ['CONFIG_CYD_WIFI_PASSWORD="keep-password"','CONFIG_CYD_API_TOKEN="keep-device-token"','CONFIG_CYD_C5_TOUCH_X_MIN=333']:self.assertIn(line,text)
   self.assertIn('CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y',text);self.assertIn('CONFIG_LV_FONT_MONTSERRAT_28=y',text);self.assertEqual((backup/'private-device-sdkconfig').read_text(),original);self.assertEqual(backup.stat().st_mode&0o777,0o700)
 def test_bad_payload_cannot_change_config(self):
  with tempfile.TemporaryDirectory() as t:
   project,bundle,cfg=self.fixture(t);original=cfg.read_text();(bundle/'payload/components/ui/settings.c').write_text('corrupt')
   with self.assertRaises(ValueError):bootstrap(bundle,project)
   self.assertEqual(cfg.read_text(),original)
 def test_antirollback_is_rejected_before_source_changes(self):
  with tempfile.TemporaryDirectory() as t:
   project,bundle,cfg=self.fixture(t);cfg.write_text(cfg.read_text()+'CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=y\n')
   with self.assertRaises(ValueError):bootstrap(bundle,project)
   self.assertFalse((project/'components/ui/settings.c').exists())
 def test_flag_edit_is_idempotent(self):
  text=configured_text('CONFIG_BOOTLOADER_LOG_LEVEL_INFO=y\n');self.assertEqual(configured_text(text),text)
 def test_wrong_target_is_rejected_before_source_changes(self):
  with tempfile.TemporaryDirectory() as t:
   project,bundle,cfg=self.fixture(t);cfg.write_text(cfg.read_text().replace('CONFIG_IDF_TARGET_ESP32C5=y','CONFIG_IDF_TARGET_ESP32S3=y'))
   with self.assertRaises(ValueError):bootstrap(bundle,project)
   self.assertFalse((project/'components/ui/settings.c').exists())

 def test_source_build_helpers_are_allowed_without_touching_build_directory(self):
  with tempfile.TemporaryDirectory() as t:
   project,bundle,cfg=self.fixture(t);p=bundle/'payload/scripts/build_ota.py';p.parent.mkdir();p.write_text('build helper');manifest=json.loads((bundle/'manifest.json').read_text());manifest['files']['scripts/build_ota.py']=hashlib.sha256(p.read_bytes()).hexdigest();(bundle/'manifest.json').write_text(json.dumps(manifest))
   bootstrap(bundle,project);self.assertEqual((project/'scripts/build_ota.py').read_text(),'build helper');self.assertTrue(cfg.exists())
