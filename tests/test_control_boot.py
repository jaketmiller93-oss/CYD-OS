"""Actual STC startup sequencer: bounded retries/recovery after OTA-style I2C faults."""
import ctypes as C
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
WRITE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8)
RESET=C.CFUNCTYPE(C.c_int,C.c_void_p)
WAIT=C.CFUNCTYPE(None,C.c_void_p,C.c_uint)
NOTE=C.CFUNCTYPE(None,C.c_void_p,C.c_uint8,C.c_uint,C.c_int)
class IO(C.Structure):
 _fields_=[('context',C.c_void_p),('write',WRITE),('reset_bus',RESET),('wait_ms',WAIT),('retry_note',NOTE)]
OK=0;TIMEOUT=0x107;STATE=0x103;ARG=0x102;FAIL=-1
class ControlBoot(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();path=Path(cls.tmp.name)/'control.so'
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/system_sdk'),str(ROOT/'components/board/control_boot.c'),'-o',str(path)],check=True)
  cls.lib=C.CDLL(str(path));cls.lib.cyd_control_boot.argtypes=[C.POINTER(IO)]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def run_start(self,writes=(),resets=()):
  self.commands=[];self.delays=[];self.notes=[];self.resets=0;w=list(writes);r=list(resets)
  def write(_,value):self.commands.append(value);return w.pop(0) if w else OK
  def reset(_):self.resets+=1;return r.pop(0) if r else OK
  def wait(_,ms):self.delays.append(ms)
  def note(_,value,attempt,error):self.notes.append((value,attempt,error))
  io=IO(None,WRITE(write),RESET(reset),WAIT(wait),NOTE(note))
  return self.lib.cyd_control_boot(C.byref(io))
 def test_success_clears_bus_before_original_commands(self):
  self.assertEqual(self.run_start(),OK);self.assertEqual(self.commands,[250,0]);self.assertEqual(self.resets,1);self.assertEqual(self.delays,[50,100])
 def test_observed_backlight_invalid_state_recovers(self):
  self.assertEqual(self.run_start([OK,STATE,OK]),OK);self.assertEqual(self.commands,[250,0,0]);self.assertEqual(self.resets,2);self.assertEqual(self.notes,[(0,1,STATE)])
 def test_touch_enable_timeout_recovers_before_backlight(self):
  self.assertEqual(self.run_start([TIMEOUT,OK,OK]),OK);self.assertEqual(self.commands,[250,250,0]);self.assertEqual(self.resets,2)
 def test_persistent_fault_is_bounded(self):
  self.assertEqual(self.run_start([TIMEOUT]*10),TIMEOUT);self.assertEqual(self.commands,[250]*3);self.assertEqual(self.resets,3);self.assertEqual(len(self.notes),2)
 def test_permanent_error_is_not_retried(self):
  self.assertEqual(self.run_start([ARG]),ARG);self.assertEqual(self.commands,[250]);self.assertEqual(self.resets,1)
 def test_initial_bus_clear_failure_prevents_writes(self):
  self.assertEqual(self.run_start(resets=[FAIL]),FAIL);self.assertEqual(self.commands,[])
 def test_retry_recovery_failure_stops(self):
  self.assertEqual(self.run_start([OK,STATE],resets=[OK,FAIL]),FAIL);self.assertEqual(self.commands,[250,0]);self.assertEqual(self.resets,2)
 def test_null_callbacks_rejected(self):
  self.assertEqual(self.lib.cyd_control_boot(None),ARG)
