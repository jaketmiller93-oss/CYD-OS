"""Actual ESP adapter with explicit lwIP/DNS/ESP-TLS test doubles."""
import ctypes as C
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class Transport(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();out=Path(cls.tmp.name)/'transport.so';subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/transport_sdk'),'-I'+str(ROOT/'components/app_services/include'),str(ROOT/'tests/transport_harness.c'),str(ROOT/'components/app_services/services.c'),'-o',str(out)],check=True);cls.lib=C.CDLL(str(out))
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def case(self,n):self.assertEqual(self.lib.test_transport_case(n),0,'ESP adapter native check line')
 def test_cancelled_dns_uses_stable_bounded_slots_and_accepts_late_callbacks(self):self.case(0)
 def test_dns_failure_does_not_start_tls(self):self.case(1)
 def test_private_ca_and_verification_name(self):self.case(2)
 def test_failed_tls_does_not_fall_back_to_plaintext(self):self.case(3)
 def test_bundle_eagain_eof_and_cleanup(self):self.case(4)
