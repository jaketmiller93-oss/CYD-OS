"""Actual UART task/lifecycle with explicit UART/RTOS doubles; no physical wiring."""
import ctypes as C
import json,os
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class Peer(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if not os.getenv('IDF_PATH'):raise unittest.SkipTest('Activate ESP-IDF for cJSON')
  cls.tmp=tempfile.TemporaryDirectory();cls.libs={};cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
  for name,flags in [('c5',[]),('s3',['-DCONFIG_IDF_TARGET_ESP32S3=1']),('disabled',['-DTEST_LINK_DISABLED']),('unconfirmed',['-DTEST_LINK_UNCONFIRMED'])]:
   out=Path(cls.tmp.name)/(name+'.so')
   subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-Wno-unused-variable','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/link_sdk'),'-I'+str(ROOT/'tests/system_sdk'),'-I'+str(ROOT/'components/device_link/include'),'-I'+str(cj)]+flags+[str(ROOT/'tests/peer_harness.c'),str(ROOT/'components/device_link/link.c'),str(cj/'cJSON.c'),'-o',str(out)],check=True)
   l=C.CDLL(str(out));l.peer_reset.argtypes=[C.c_int,C.c_int,C.c_int];l.peer_status_json.argtypes=[C.c_void_p,C.c_size_t];cls.libs[name]=l
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def status(self,l):
  b=C.create_string_buffer(2048);self.assertTrue(l.peer_status_json(b,len(b)));return json.loads(b.value)
 def test_disabled_initializes_no_uart(self):
  l=self.libs['disabled'];l.peer_reset(0,4,5);self.assertEqual(l.peer_start(),0);self.assertEqual(l.peer_counters(0),0);self.assertFalse(self.status(l)['configured']);self.assertEqual(self.status(l)['role'],'c5_connectivity_companion')
 def test_unconfirmed_wiring_initializes_no_uart(self):
  l=self.libs['unconfirmed'];l.peer_reset(0,4,5);self.assertNotEqual(l.peer_start(),0);self.assertEqual(l.peer_counters(0),0);self.assertFalse(self.status(l)['running'])
 def test_unset_conflicting_and_reserved_pins(self):
  for name,pairs in [('c5',[(-1,5),(4,4),(13,5),(4,23),(7,5)]),('s3',[(19,2),(1,20),(39,2),(1,15),(1,36),(1,2),(8,1)])]:
   l=self.libs[name]
   for tx,rx in pairs:l.peer_reset(0,tx,rx);self.assertNotEqual(l.peer_start(),0);self.assertEqual(l.peer_counters(0),0)
 def test_sdk_failure_cleanup(self):
  l=self.libs['c5']
  for failure in range(1,6):
   l.peer_reset(failure,4,5);self.assertNotEqual(l.peer_start(),0);self.assertFalse(self.status(l)['running']);self.assertEqual(l.peer_counters(1),int(failure in [4,5]));self.assertEqual(l.peer_counters(2),int(failure==5));self.assertEqual(l.peer_counters(4),1)
 def test_external_protocol_memory_failure_does_not_initialize_uart(self):
  for name,pins in [('c5',(5,4)),('s3',(8,2))]:
   l=self.libs[name];l.peer_reset(6,*pins);self.assertNotEqual(l.peer_start(),0);self.assertEqual(l.peer_counters(0),0);self.assertEqual(l.peer_counters(3),1);self.assertEqual(l.peer_counters(4),0);self.assertFalse(self.status(l)['running'])
 def test_actual_worker_with_partial_tx_fragmented_rx_and_blocked_fifo(self):
  for name,pins in [('c5',(5,4)),('s3',(8,2))]:
   l=self.libs[name];l.peer_reset(0,*pins);self.assertEqual(l.peer_start(),0);self.assertTrue(l.peer_run());s=self.status(l);self.assertTrue(s['connected']);self.assertTrue(s['remote_status_valid']);self.assertEqual(s['remote']['firmware'],'remote-fixture');self.assertEqual(s['remote']['free_heap'],234567);self.assertEqual(s['role'],'s3_application_host' if name=='s3' else 'c5_connectivity_companion');self.assertEqual(s['last_echo_result'],0);self.assertFalse(s['service_offload']);self.assertFalse(s['authenticated']);self.assertNotEqual(l.peer_start(),0)
