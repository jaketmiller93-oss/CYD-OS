"""Native tests of the real OTA, settings, credential and SD catalog services."""
import ctypes as C
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class SystemServices(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  idf=os.getenv('IDF_PATH')
  if not idf:raise unittest.SkipTest('Activate ESP-IDF for cJSON sources')
  cls.tmp=tempfile.TemporaryDirectory();out=Path(cls.tmp.name);cj=Path(idf)/'components/json/cJSON';cls.libs={}
  base=['cc','-shared','-fPIC','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/system_sdk'),'-I'+str(cj)]
  for kind in ['ota','device','access','catalog']:
   files=[ROOT/'tests/system_runtime.c',ROOT/f'tests/{kind}_harness.c'];includes=[]
   for component in ['ota_manager','device_settings','device_access','app_manager']:includes+=['-I'+str(ROOT/'components'/component/'include')]
   if kind=='ota':files+=[ROOT/'components/ota_manager/policy.c']
   if kind=='device':files+=[ROOT/'components/device_settings/geometry.c']
   if kind=='catalog':files+=[ROOT/'components/cyd_common/zip.c',ROOT/'components/cyd_common/json.c',cj/'cJSON.c']
   subprocess.run(base+includes+[str(f) for f in files]+['-lm','-o',str(out/(kind+'.so'))],check=True)
   cls.libs[kind]=C.CDLL(str(out/(kind+'.so')))
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def case(self,kind,n):self.assertEqual(getattr(self.libs[kind],f'host_{kind}_case')(n),0,f'{kind} native check line')
 def test_ota_requires_confirmed_boot(self):self.case('ota',0)
 def test_ota_selects_only_after_verified_explicit_restart(self):self.case('ota',1)
 def test_ota_disconnect_retains_running_selection(self):self.case('ota',2)
 def test_ota_write_failure_aborts(self):self.case('ota',3)
 def test_ota_sdk_image_integrity_failure(self):self.case('ota',4)
 def test_ota_digest_failure(self):self.case('ota',5)
 def test_ota_healthy_pending_boot_confirmed(self):self.case('ota',6)
 def test_ota_stale_services_roll_back_pending_boot(self):self.case('ota',7)
 def test_ota_lua_stop_failure_cannot_erase(self):self.case('ota',8)
 def test_ota_wrong_chip_cannot_erase(self):self.case('ota',9)
 def test_ota_cancel_does_not_select(self):self.case('ota',10)
 def test_ota_restart_task_failure_can_cancel(self):self.case('ota',11)
 def test_display_rotation_timeout_restores_saved(self):self.case('device',0)
 def test_display_confirm_survives_restart(self):self.case('device',1)
 def test_display_failed_save_keeps_revert_timeout(self):self.case('device',2)
 def test_calibration_hold_cancels_without_mapping(self):self.case('device',3)
 def test_calibration_five_targets_fit_and_persist(self):self.case('device',4)
 def test_calibration_timeout_restores(self):self.case('device',5)
 def test_brightness_persists_and_rejects_zero(self):self.case('device',6)
 def test_unconfirmed_orientation_cannot_be_saved_by_brightness(self):self.case('device',7)
 def test_access_import_and_restart(self):self.case('access',0)
 def test_access_rotation_invalidates_old_token(self):self.case('access',1)
 def test_access_failed_rotation_preserves_working_token(self):self.case('access',2)
 def test_access_rejects_partial_and_wrong_bearer(self):self.case('access',3)
 def test_access_corrupt_record_not_overwritten(self):self.case('access',4)
 def test_catalog_groups_versions(self):self.case('catalog',0)
 def test_catalog_folder_move_reload_delete_keeps_apps(self):self.case('catalog',1)
 def test_catalog_rename_persists(self):self.case('catalog',2)
 def test_catalog_corrupt_latest_falls_back(self):self.case('catalog',3)
 def test_catalog_failed_write_keeps_state(self):self.case('catalog',4)
 def test_catalog_invalid_names_paths_and_missing_apps(self):self.case('catalog',5)
 def test_catalog_maintenance_blocks_mutation(self):self.case('catalog',6)
 def test_catalog_bounded_folder_count(self):self.case('catalog',7)
 def policy(self,p,total=1024):
  lib=self.libs['ota'];fn=lib.cyd_ota_policy_validate;fn.argtypes=[C.c_void_p,C.c_size_t,C.c_size_t,C.c_uint16,C.c_char_p,C.c_char_p,C.c_void_p,C.c_size_t];fn.restype=C.c_bool
  return fn(bytes(p),len(p),total,0x17,b'nm_cyd_c5',b'a'*64,C.create_string_buffer(160),160)
 def prefix(self):
  p=bytearray(512);p[0]=0xe9;p[1]=3;p[3]=0x40;p[12]=0x17;p[23]=1;struct.pack_into('<I',p,28,1024);struct.pack_into('<I',p,32,0xabcd5432);p[48:54]=b'0.3.1\0';p[80:87]=b'cyd-os\0';struct.pack_into('<16sI24sIIIII65s63s',p,288,b'CYD-OS-UPDATE',1,b'nm_cyd_c5',7,1,1,1,1,b'a'*64,b'');return p
 def test_policy_valid_image(self):self.assertTrue(self.policy(self.prefix()))
 def test_policy_rejects_wrong_hardware_and_descriptor(self):
  for offset in [0,1,3,12,23,32,80,288,304,308,332,336,340,344,348,352]:
   p=self.prefix();p[offset]^=255
   with self.subTest(offset=offset):self.assertFalse(self.policy(p))
 def test_policy_rejects_size_and_truncation(self):
  self.assertFalse(self.policy(self.prefix(),0x700001));self.assertFalse(self.policy(self.prefix()[:511]));self.assertFalse(self.policy(self.prefix(),511))
 def test_policy_rejects_unterminated_version(self):
  p=self.prefix();p[48:80]=b'x'*32;self.assertFalse(self.policy(p))
 def test_rotation_touch_and_pixel_mapping_agree(self):
  lib=self.libs['device'];fn=lib.cyd_rotate_rgb565;fn.argtypes=[C.c_void_p,C.c_void_p,C.c_uint,C.c_uint,C.c_uint]
  for rotation in range(4):
   w,h=5,3;logical_w,logical_h=(h,w) if rotation%2 else (w,h);src=(C.c_uint16*(w*h))(*range(w*h));out=(C.c_uint16*(w*h))();fn(src,out,logical_w,logical_h,rotation)
   for y in range(h):
    for x in range(w):
     lx,ly=C.c_int(),C.c_int();lib.cyd_display_to_logical(rotation,w,h,x,y,C.byref(lx),C.byref(ly));self.assertEqual(out[y*w+x],src[ly.value*logical_w+lx.value]);nx,ny=C.c_int(),C.c_int();lib.cyd_display_to_native(rotation,w,h,lx,ly,C.byref(nx),C.byref(ny));self.assertEqual((nx.value,ny.value),(x,y))
 def test_clock_leap_year_and_utc_offset(self):
  from datetime import datetime,timezone
  fn=self.libs['device'].cyd_clock_parse_local;fn.argtypes=[C.c_char_p,C.c_int,C.POINTER(C.c_int64)];fn.restype=C.c_bool;stamp=C.c_int64()
  self.assertTrue(fn(b'2024-02-29 15:45',-300,C.byref(stamp)));self.assertEqual(stamp.value,int(datetime(2024,2,29,20,45,tzinfo=timezone.utc).timestamp()))
  for bad in [b'2025-02-29 15:45',b'2024-13-01 00:00',b'2024-04-31 00:00',b'2024-02-29 24:00',b'2024-02-29 15:60',b'2024-02-29 15:45x']:
   self.assertFalse(fn(bad,0,C.byref(stamp)))

 def test_policy_rejects_short_descriptor_segment(self):
  p=self.prefix();struct.pack_into("<I",p,28,200);self.assertFalse(self.policy(p))

 def test_bootstrap_calibration_persisted_before_first_ota(self):self.case("device",8)
