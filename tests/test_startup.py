"""Required API startup precedes optional UART; no actual hardware allocation."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
class Startup(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp = tempfile.TemporaryDirectory()
  out = Path(cls.tmp.name) / 'startup.so'
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror',
    '-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/startup_sdk'),
    str(ROOT/'tests/startup_harness.c'),'-o',str(out)],check=True)
  cls.lib = ctypes.CDLL(str(out))
 @classmethod
 def tearDownClass(cls): cls.tmp.cleanup()
 def test_web_server_reserves_resources_before_optional_peer(self):
  self.assertEqual(self.lib.startup_case(0),0)
 def test_failed_api_remains_unhealthy_and_logs_error_and_internal_block(self):
  self.assertEqual(self.lib.startup_case(1),0)
 def test_failed_optional_peer_does_not_remove_api_health(self):
  self.assertEqual(self.lib.startup_case(2),0)

class PairedStartup(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp = tempfile.TemporaryDirectory()
  out = Path(cls.tmp.name) / 'paired-startup.so'
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror',
    '-DCONFIG_CYD_PAIRED_SYSTEM=1','-DCONFIG_IDF_TARGET_ESP32C5=1',
    '-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/startup_sdk'),
    str(ROOT/'tests/startup_harness.c'),'-o',str(out)],check=True)
  cls.lib = ctypes.CDLL(str(out))
 @classmethod
 def tearDownClass(cls): cls.tmp.cleanup()
 def test_web_reserves_resources_before_link_and_services(self):
  self.assertEqual(self.lib.startup_case(0),0)
 def test_web_failure_keeps_startup_unconfirmed(self):
  self.assertEqual(self.lib.startup_case(1),0)
 def test_uart_failure_keeps_startup_unconfirmed_with_exact_error(self):
  self.assertEqual(self.lib.startup_case(2),0)
 def test_service_failure_keeps_startup_unconfirmed_with_exact_error(self):
  self.assertEqual(self.lib.startup_case(3),0)
 def test_route_registration_failure_keeps_startup_unconfirmed(self):
  self.assertEqual(self.lib.startup_case(4),0)
 def test_http_task_is_reserved_before_ui_can_fragment_internal_heap(self):
  self.assertEqual(self.lib.startup_case(5),0)
