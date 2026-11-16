"""Board-specific default version strings, independent from app API/OTA protocol."""
from pathlib import Path
import importlib.util,unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('release_build',ROOT/'scripts/build_ota.py')
build=importlib.util.module_from_spec(spec);spec.loader.exec_module(build)
class ReleaseLines(unittest.TestCase):
 def test_s3_default(self):self.assertEqual(build.default_version('crowpanel_s3'),'0.4.3s')
 def test_c5_baseline_default(self):self.assertEqual(build.default_version('nm_cyd_c5'),'0.4.4c')
 def test_unknown_board_is_rejected(self):
  with self.assertRaises(ValueError):build.default_version('../crowpanel_s3')
 def test_link_profile_follows_the_actual_crossed_cable(self):
  s=build.link_config('crowpanel_s3',build.LINK_PROFILE)
  c=build.link_config('nm_cyd_c5',build.LINK_PROFILE)
  self.assertEqual((s['CONFIG_CYD_LINK_TX_GPIO'],s['CONFIG_CYD_LINK_RX_GPIO']),('8','2'))
  self.assertEqual((c['CONFIG_CYD_LINK_TX_GPIO'],c['CONFIG_CYD_LINK_RX_GPIO']),('5','4'))
  for cfg in [s,c]:
   self.assertEqual(cfg['CONFIG_CYD_LINK_ENABLE'],'y')
   self.assertEqual(cfg['CONFIG_CYD_LINK_BAUD'],'115200')
 def test_standalone_does_not_enable_uart(self):
  build.verify_link_config('# CONFIG_CYD_LINK_ENABLE is not set\n',build.link_config('crowpanel_s3'))
  with self.assertRaises(ValueError):
   build.verify_link_config('CONFIG_CYD_LINK_ENABLE=y\n',build.link_config('crowpanel_s3'))
 def test_wrong_or_stale_pin_profile_is_rejected(self):
  cfg=build.link_config('crowpanel_s3',build.LINK_PROFILE)
  good=''.join(k+'='+v+'\n' for k,v in cfg.items())
  build.verify_link_config(good,cfg)
  with self.assertRaises(ValueError):build.verify_link_config(good.replace('GPIO=8','GPIO=19'),cfg)
  with self.assertRaises(ValueError):build.link_config('crowpanel_s3','unknown')
