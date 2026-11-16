"""Actual UART adapter must discard queued calls from a retired peer session."""
import ctypes as C
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class PairPeer(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if not os.getenv('IDF_PATH'):raise unittest.SkipTest('ESP-IDF cJSON required')
  cls.tmp=tempfile.TemporaryDirectory();cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON';out=Path(cls.tmp.name)/'peer.so'
  includes=[ROOT/'tests/link_sdk',ROOT/'tests/system_sdk',ROOT/'components/device_link/include',cj]
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-Wno-unused-variable','-DCONFIG_CYD_PAIRED_SYSTEM=1','-DCYD_LINK_ENABLE_RPC=1','-fsanitize=undefined','-fno-sanitize-recover=all']+['-I'+str(p)for p in includes]+[str(ROOT/'tests/peer_harness.c'),str(ROOT/'components/device_link/link.c'),str(cj/'cJSON.c'),'-o',str(out)],check=True);cls.lib=C.CDLL(str(out));cls.lib.peer_rpc_failure_case.argtypes=[C.c_int]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def test_retired_queued_requests_cannot_execute(self):self.assertEqual(self.lib.peer_queued_epoch_case(),1)
 def test_rpc_resource_allocation_and_worker_failure_cleanup(self):
  for failure in [5,7,8,9]:self.assertEqual(self.lib.peer_rpc_failure_case(failure),1)
