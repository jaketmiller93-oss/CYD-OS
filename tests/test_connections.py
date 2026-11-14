"""Run the real network service against native radio/NVS/task test doubles."""
import ctypes as C
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Profile(C.Structure): _fields_=[('ssid',C.c_char*33),('password',C.c_char*65)]
class Config(C.Structure): _fields_=[('version',C.c_uint32),('count',C.c_uint8),('preferred',C.c_int8),('station',C.c_uint8),('ap',C.c_uint8),('ap_ssid',C.c_char*33),('ap_password',C.c_char*64),('profiles',Profile*5)]
class Scan(C.Structure): _fields_=[('ssid',C.c_char*33),('rssi',C.c_int8),('secured',C.c_bool),('supported',C.c_bool)]
class Status(C.Structure): _fields_=[('ready',C.c_bool),('busy',C.c_bool),('scanning',C.c_bool),('station',C.c_bool),('ap',C.c_bool),('state',C.c_int),('revision',C.c_uint32),('scan_revision',C.c_uint32),('ssid',C.c_char*33),('station_ip',C.c_char*16),('ap_ssid',C.c_char*33),('ap_ip',C.c_char*16),('message',C.c_char*128),('saved_count',C.c_uint),('scan_count',C.c_uint),('saved',(C.c_char*33)*5),('scan',Scan*16)]
class Connections(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();lib=Path(cls.tmp.name)/'network.so'
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/network_sdk'),'-I'+str(ROOT/'components/network/include'),str(ROOT/'tests/network_harness.c'),str(ROOT/'components/network/policy.c'),'-o',str(lib)],check=True)
  cls.lib=C.CDLL(str(lib));l=cls.lib
  l.host_boot.argtypes=[C.c_bool];l.host_tick.argtypes=[C.c_uint];l.host_ip.argtypes=[C.c_char_p];l.host_config.argtypes=[C.POINTER(Config)];l.cyd_network_snapshot.argtypes=[C.POINTER(Status)];l.host_fail_commit.argtypes=[C.c_bool];l.host_delayed_disconnect.argtypes=[C.c_bool];l.host_queue_ip.argtypes=[C.c_char_p,C.c_uint32]
  l.cyd_network_join.argtypes=[C.c_char_p,C.c_char_p];l.cyd_network_join_saved.argtypes=[C.c_char_p];l.cyd_network_forget.argtypes=[C.c_char_p];l.cyd_network_set_ap.argtypes=[C.c_char_p,C.c_char_p,C.c_bool,C.c_bool];l.cyd_wifi_config_valid.argtypes=[C.POINTER(Config)];l.cyd_wifi_config_valid.restype=C.c_bool
  assert C.sizeof(Config)==l.host_config_size();assert C.sizeof(Status)==l.host_status_size()
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def setUp(self):self.assertEqual(self.lib.host_boot(False),0)
 def config(self):c=Config();self.lib.host_config(C.byref(c));return c
 def status(self):s=Status();self.lib.cyd_network_snapshot(C.byref(s));return s
 def join(self,ssid=b'Phone',password=b'phone-password'):
  self.assertEqual(self.lib.cyd_network_join(ssid,password),0);self.lib.host_pump()
 def test_imports_existing_wifi_once_and_generates_protected_ap(self):
  c=self.config();self.assertEqual(c.count,1);self.assertEqual(c.profiles[0].ssid,b'Home');self.assertEqual(c.profiles[0].password,b'home-password');self.assertEqual(c.ap,0);self.assertEqual(len(c.ap_password),16)
 def test_credentials_saved_only_after_dhcp(self):
  self.join();self.assertEqual(self.config().count,1);self.assertEqual(self.status().state,2)
  self.lib.host_ip(b'Phone');c=self.config();self.assertEqual(c.count,2);self.assertEqual(c.preferred,1);self.assertEqual(self.status().station_ip,b'192.168.1.100')
 def test_association_to_wrong_network_cannot_confirm_startup(self):
  self.join();self.lib.host_ip(b'Home');self.assertEqual(self.config().count,1);self.assertEqual(self.status().state,2)
 def test_bad_password_timeout_preserves_known_credentials(self):
  self.join(b'Home',b'wrong-password');self.lib.host_tick(16000)
  self.assertEqual(self.status().state,4);self.assertEqual(self.config().profiles[0].password,b'home-password')
  self.assertEqual(self.lib.host_boot(True),0);self.assertEqual(self.config().profiles[0].password,b'home-password')
 def test_successful_network_survives_reboot_and_is_preferred(self):
  self.join();self.lib.host_ip(b'Phone');self.assertEqual(self.lib.host_boot(True),0)
  self.assertEqual(self.config().count,2);self.lib.host_tick(0);self.assertEqual(self.status().ssid,b'Phone')
 def test_nvs_failure_keeps_previous_working_password(self):
  self.join(b'Home',b'new-password');self.lib.host_fail_commit(True);self.lib.host_ip(b'Home')
  self.assertEqual(self.status().state,3);self.assertIn(b'saving',self.status().message);self.assertEqual(self.config().profiles[0].password,b'home-password')
 def test_ap_only_and_resume_saved_station(self):
  self.assertEqual(self.lib.cyd_network_set_ap(b'CYD-Pocket',b'private-ap-password',True,True),0);self.lib.host_pump()
  self.assertEqual(self.lib.host_mode(),2);self.assertEqual(self.config().station,0);self.assertEqual(self.status().ap_ip,b'192.168.4.1')
  self.assertEqual(self.lib.cyd_network_join_saved(b'Home'),0);self.lib.host_pump();self.assertEqual(self.lib.host_mode(),3);self.lib.host_ip(b'Home');self.assertTrue(self.status().ap)
 def test_scan_deduplicates_and_does_not_enable_station_permanently(self):
  self.lib.cyd_network_disconnect();self.lib.host_pump();before=self.status().scan_revision
  self.lib.cyd_network_scan();self.lib.host_pump();s=self.status()
  self.assertEqual(s.scan_count,3);self.assertGreater(s.scan_revision,before);self.assertEqual(self.config().station,0);self.assertFalse(s.scan[2].secured)
 def test_forget_active_network_disconnects_and_removes_profile(self):
  self.lib.host_tick(0);self.lib.host_ip(b'Home');self.lib.cyd_network_forget(b'Home');self.lib.host_pump()
  self.assertEqual(self.config().count,0);self.assertEqual(self.config().preferred,-1);self.assertEqual(self.config().station,0);self.assertEqual(self.status().station_ip,b'')
 def test_saved_profiles_are_bounded_without_eviction(self):
  for i in range(4):name=f'Network{i}'.encode();self.join(name);self.lib.host_ip(name)
  count=self.lib.host_attempts();self.join(b'Sixth');self.assertEqual(self.config().count,5);self.assertEqual(self.lib.host_attempts(),count);self.assertIn(b'forget',self.status().message)
 def test_disconnect_keeps_profiles_and_stays_off_across_reboot(self):
  self.lib.cyd_network_disconnect();self.lib.host_pump();self.lib.host_boot(True);self.lib.host_tick(60000)
  self.assertEqual(self.config().count,1);self.assertEqual(self.lib.host_attempts(),0)
 def test_public_status_never_contains_saved_passwords(self):
  self.join();self.lib.host_ip(b'Phone');raw=bytes(self.status());self.assertNotIn(b'home-password',raw);self.assertNotIn(b'phone-password',raw);self.assertNotIn(self.config().ap_password,raw)
 def test_reconnect_tries_other_saved_network_after_failure(self):
  self.join();self.lib.host_ip(b'Phone');self.lib.host_boot(True);self.lib.host_tick(0)
  self.assertEqual(self.status().ssid,b'Phone');self.lib.host_tick(16000);self.lib.host_tick(4000);self.assertEqual(self.status().ssid,b'Home')
 def test_stale_dhcp_cannot_save_a_new_password(self):
  self.lib.host_tick(0);self.lib.host_ip(b'Home');self.lib.host_delayed_disconnect(True)
  self.assertEqual(self.lib.cyd_network_join(b'Home',b'wrong-password'),0)
  self.lib.host_queue_ip(b'Home',0x6401a8c0);self.lib.host_pump()
  self.assertEqual(self.status().state,2);self.assertEqual(self.config().profiles[0].password,b'home-password')
 def test_dhcp_renewal_updates_displayed_ip(self):
  self.lib.host_tick(0);self.lib.host_ip(b'Home');before=bytes(self.config())
  self.lib.host_queue_ip(b'Home',0x6501a8c0);self.lib.host_pump()
  self.assertEqual(self.status().station_ip,b'192.168.1.101');self.assertEqual(bytes(self.config()),before)
 def test_credentials_validation_before_queueing(self):
  for ssid,password in [(b'',b'password'),(b'x'*33,b'password'),(b'Home',b'short'),(b'Home',b'z'*64)]:self.assertNotEqual(self.lib.cyd_network_join(ssid,password),0)
  self.assertNotEqual(self.lib.cyd_network_set_ap(b'CYD',b'',True,False),0)
  self.assertEqual(self.lib.cyd_network_join(b'Home',b'a'*64),0)
 def test_malformed_persisted_configuration_is_rejected(self):
  for field,value in [('version',99),('count',6),('preferred',5),('station',2),('ap',2)]:
   c=self.config();setattr(c,field,value);self.assertFalse(self.lib.cyd_wifi_config_valid(C.byref(c)))

if __name__=='__main__':unittest.main()
