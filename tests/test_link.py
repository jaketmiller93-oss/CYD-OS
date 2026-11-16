"""Executes the portable paired-device engine; PTY is not physical UART hardware."""
import ctypes as C
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import tty
import unittest
import zlib
ROOT=Path(__file__).resolve().parents[1]
class Health(C.Structure):
 _fields_=[('uptime_ms',C.c_uint64),('free_heap',C.c_uint32),('sd_ready',C.c_bool),('network_address',C.c_bool),('version',C.c_char*32)]
class Event(C.Structure):
 _fields_=[('id',C.c_uint32),('result',C.c_int),('size',C.c_size_t),('data',C.c_ubyte*256)]
def encode(raw):
 out=bytearray([0]);pos=0;code=1
 for b in raw:
  if not b:out[pos]=code;pos=len(out);out.append(0);code=1
  else:
   out.append(b);code+=1
   if code==255:out[pos]=code;pos=len(out);out.append(0);code=1
 out[pos]=code;out.append(0);return bytes(out)
def frame(kind,role,boot,identity=0,payload=b'',version=1):
 raw=struct.pack('<BBBBQIH',version,kind,role,0,boot,identity,len(payload))+payload
 return encode(raw+struct.pack('<I',zlib.crc32(raw)))
class Link(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();out=Path(cls.tmp.name)
  sources=[str(ROOT/'components/device_link/link.c'),str(ROOT/'tests/link_harness.c')]
  common=['cc','-Wall','-Wextra','-Werror','-I'+str(ROOT/'components/device_link/include')]
  subprocess.run(common+['-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all']+sources+['-o',str(out/'link.so')],check=True)
  subprocess.run(common+['-DLINK_FUZZ_MAIN','-fsanitize=address,undefined','-fno-sanitize-recover=all']+sources+['-o',str(out/'fuzz')],check=True)
  cls.fuzz=out/'fuzz';cls.lib=C.CDLL(str(out/'link.so'));l=cls.lib
  l.link_reset.argtypes=[C.c_uint,C.c_int,C.c_uint64];l.link_tick.argtypes=[C.c_uint,C.c_uint64];l.link_feed.argtypes=[C.c_uint,C.c_void_p,C.c_size_t];l.link_tx.argtypes=[C.c_uint,C.c_void_p,C.c_size_t];l.link_tx.restype=C.c_size_t;l.link_ping.argtypes=[C.c_uint,C.c_void_p,C.c_size_t,C.c_uint];l.link_ping.restype=C.c_uint32;l.link_cancel.argtypes=[C.c_uint,C.c_uint32];l.link_event.argtypes=[C.c_uint,C.POINTER(Event)];l.link_serial.argtypes=[C.c_uint,C.c_uint32]
  l.link_health_send.argtypes=[C.c_uint,C.POINTER(Health)];l.link_health_get.argtypes=[C.c_uint,C.POINTER(Health),C.POINTER(C.c_uint32)];l.link_health_serial.argtypes=[C.c_uint,C.c_uint32]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def setUp(self):self.assertTrue(self.lib.link_reset(0,1,11));self.assertTrue(self.lib.link_reset(1,2,22))
 def feed(self,node,data,chunk=0):
  for start in range(0,len(data),chunk or len(data)):self.lib.link_feed(node,data[start:start+(chunk or len(data))],len(data[start:start+(chunk or len(data))]))
 def tx(self,node):
  b=C.create_string_buffer(512);n=self.lib.link_tx(node,b,len(b));return b.raw[:n]
 def pump(self,chunk=0):
  for _ in range(20):
   moved=False
   for node in range(2):
    while True:
     b=self.tx(node)
     if not b:break
     self.feed(1-node,b,chunk);moved=True
   if not moved:return
  self.fail('handshake failed to quiesce')
 def connect(self,chunk=0):self.lib.link_tick(0,0);self.lib.link_tick(1,0);self.pump(chunk);self.assertTrue(self.lib.link_ready(0));self.assertTrue(self.lib.link_ready(1))
 def event(self,node=0):
  e=Event();return e if self.lib.link_event(node,C.byref(e)) else None
 def test_handshake_and_binary_fragmented_echo(self):
  self.connect(1);body=bytes(range(256));identity=self.lib.link_ping(0,body,len(body),1000);self.assertTrue(identity);self.pump(1);e=self.event();self.assertEqual((e.id,e.result,bytes(e.data[:e.size])),(identity,0,body))
 def test_independent_reference_cobs_crc_frames(self):
  self.feed(0,frame(1,2,22,payload=struct.pack('<I',1)),1);self.feed(0,frame(2,2,22,payload=struct.pack('<QI',11,1)),1);self.assertTrue(self.lib.link_ready(0));identity=self.lib.link_ping(0,b'x',1,500);self.feed(0,frame(4,2,22,identity,b'x'));self.assertEqual(self.event().result,0)
 def test_ack_before_hello_and_wrong_boot_echo(self):
  self.feed(0,frame(2,2,22,payload=struct.pack('<QI',99,1)));self.assertFalse(self.lib.link_ready(0));self.feed(0,frame(2,2,22,payload=struct.pack('<QI',11,1)));self.assertTrue(self.lib.link_ready(0))
 def test_role_protocol_and_bad_checksum_rejected(self):
  for bad in [frame(1,1,22,payload=struct.pack('<I',1)),frame(1,2,22,payload=struct.pack('<I',1),version=2),frame(1,2,22,payload=struct.pack('<I',0))]:self.feed(0,bad)
  b=bytearray(frame(1,2,22,payload=struct.pack('<I',1)));b[-3]^=1;self.feed(0,bytes(b));self.assertFalse(self.lib.link_ready(0));self.assertGreaterEqual(self.lib.link_bad(0),4)
 def test_oversize_and_noise_resynchronize(self):
  self.feed(0,b'\x01'*2000+b'\0');self.feed(0,b'bad\0\0\x05x\0');self.assertGreater(self.lib.link_bad(0),0);self.connect(3)
 def test_transmit_queue_bound_and_small_destination(self):
  for now in range(0,3000,250):self.lib.link_tick(0,now)
  self.assertGreater(self.lib.link_dropped(0),0);small=C.create_string_buffer(1);self.assertEqual(self.lib.link_tx(0,small,1),0)
  frames=[]
  while True:
   data=self.tx(0)
   if not data:break
   frames.append(data)
  self.assertEqual(len(frames),8);self.assertTrue(all(len(f)<=282 and f.endswith(b'\0') for f in frames))
 def test_oversized_payload_frame_never_delivers(self):
  self.connect();self.feed(0,frame(3,2,22,99,b'x'*257));self.assertGreater(self.lib.link_bad(0),0);self.assertIsNone(self.event());self.assertTrue(self.lib.link_ready(0))
 def test_request_limits_and_capacity(self):
  self.assertFalse(self.lib.link_ping(0,b'x',1,1000));self.connect()
  self.assertFalse(self.lib.link_ping(0,b'x'*257,257,1000));self.assertFalse(self.lib.link_ping(0,b'x',1,99));self.assertFalse(self.lib.link_ping(0,b'x',1,5001))
  ids=[self.lib.link_ping(0,b'x',1,1000) for _ in range(4)];self.assertTrue(all(ids));self.assertEqual(len(set(ids)),4);self.assertFalse(self.lib.link_ping(0,b'x',1,1000));self.pump();self.assertEqual(len([self.event() for _ in range(4)]),4)
 def test_timeout_and_late_reply(self):
  self.connect();identity=self.lib.link_ping(0,b'x',1,100);self.lib.link_tick(0,100);e=self.event();self.assertEqual((e.id,e.result),(identity,1));self.pump();self.assertIsNone(self.event())
 def test_cancel_pending_and_queued_completion(self):
  self.connect();identity=self.lib.link_ping(0,b'x',1,1000);self.assertTrue(self.lib.link_cancel(0,identity));self.pump();self.assertIsNone(self.event());identity=self.lib.link_ping(0,b'x',1,1000);self.pump();self.assertTrue(self.lib.link_cancel(0,identity));self.assertIsNone(self.event())
 def test_heartbeat_loss_fails_pending(self):
  self.connect();identity=self.lib.link_ping(0,b'x',1,5000);self.lib.link_tick(0,3500);self.assertFalse(self.lib.link_ready(0));e=self.event();self.assertEqual((e.id,e.result),(identity,2))
 def test_peer_reset_rejects_stale_replies(self):
  self.connect();identity=self.lib.link_ping(0,b'old',3,1000);old_reply=frame(4,2,22,identity,b'old');self.assertTrue(self.lib.link_reset(1,2,33));self.lib.link_tick(1,10);self.pump();self.lib.link_tick(0,10);self.pump();self.assertTrue(self.lib.link_ready(0));e=self.event();self.assertEqual((e.id,e.result),(identity,3));self.feed(0,old_reply);self.feed(0,frame(1,2,22,payload=struct.pack('<I',1)));self.assertTrue(self.lib.link_ready(0));self.assertIsNone(self.event());new=self.lib.link_ping(0,b'new',3,1000);self.assertNotEqual(new,identity);self.pump();self.assertEqual(bytes(self.event().data[:3]),b'new')
 def test_already_queued_success_becomes_reset_failure(self):
  self.connect();identity=self.lib.link_ping(0,b'x',1,1000);self.pump();self.feed(0,frame(1,2,33,payload=struct.pack('<I',1)));e=self.event();self.assertEqual((e.id,e.result,e.size),(identity,3,0))
 def test_completed_queue_backpressure_and_sequence_exhaustion(self):
  self.connect()
  for _ in range(4):self.assertTrue(self.lib.link_ping(0,b'x',1,1000))
  self.pump()
  for _ in range(4):self.assertTrue(self.lib.link_ping(0,b'y',1,1000))
  self.pump();self.assertEqual(self.lib.link_pending(0),4);ids=[self.event().id for _ in range(8)];self.assertEqual(len(set(ids)),8);self.assertIsNone(self.event());self.lib.link_serial(0,0xffffffff);self.assertFalse(self.lib.link_ping(0,b'x',1,1000))
 def test_real_posix_pseudo_terminal_duplex(self):
  a,b=os.openpty();tty.setraw(b);os.set_blocking(a,False);os.set_blocking(b,False)
  try:
   def wire():
    for node,fd in enumerate((a,b)):
     while True:
      packet=self.tx(node)
      if not packet:break
      self.assertEqual(os.write(fd,packet),len(packet))
    for node,fd in enumerate((a,b)):
     while True:
      try:packet=os.read(fd,7)
      except BlockingIOError:break
      if not packet:break
      self.feed(node,packet,2)
   for now in range(100):self.lib.link_tick(0,now);self.lib.link_tick(1,now);wire()
   self.assertTrue(self.lib.link_ready(0));self.assertTrue(self.lib.link_ready(1));body=b'virtual UART\0binary';identity=self.lib.link_ping(0,body,len(body),1000)
   for _ in range(100):wire()
   e=self.event();self.assertIsNotNone(e);self.assertEqual((e.id,bytes(e.data[:e.size])),(identity,body))
  finally:os.close(a);os.close(b)
 def test_native_sanitizer_malformed_traffic(self):
  result=subprocess.run([str(self.fuzz)],capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stderr)

 def health(self,node):
  h=Health();age=C.c_uint32();return (h,age.value) if self.lib.link_health_get(node,C.byref(h),C.byref(age)) else None
 def send_health(self,node=0):
  h=Health(1234567890123,456789,True,False,b'0.4.1-pair.1');return self.lib.link_health_send(node,C.byref(h))
 def status_frame(self,identity=1,boot=22,target=11,version=b'peer',flags=3,schema=1):
  body=struct.pack('<QBBHQI32s',target,schema,flags,0,9876543210,123456,version)
  return frame(5,2,boot,identity,body)
 def test_status_two_board_exchange_fragmented(self):
  self.assertFalse(self.send_health());self.connect(1);self.assertTrue(self.send_health());self.assertTrue(self.send_health(1));self.pump(1)
  for node in [0,1]:
   h,age=self.health(node);self.assertEqual((h.uptime_ms,h.free_heap,h.sd_ready,h.network_address,h.version,age),(1234567890123,456789,True,False,b'0.4.1-pair.1',0));self.assertIsNone(self.event(node))
 def test_status_independent_reference_and_expiry_despite_heartbeat(self):
  self.connect();self.feed(0,self.status_frame(),1);h,age=self.health(0);self.assertEqual(h.version,b'peer');self.lib.link_tick(0,2999);self.assertEqual(self.health(0)[1],2999)
  self.feed(0,frame(2,2,22,payload=struct.pack('<QI',11,3)));self.lib.link_tick(0,3000);self.assertTrue(self.lib.link_ready(0));self.assertIsNone(self.health(0))
 def test_status_rejects_wrong_receiver_session_and_nonincreasing_sequence(self):
  self.connect();self.feed(0,self.status_frame(target=99));self.assertIsNone(self.health(0));self.feed(0,self.status_frame(identity=2));self.lib.link_tick(0,100);self.feed(0,self.status_frame(identity=1,version=b'stale'));self.feed(0,self.status_frame(identity=2,version=b'duplicate'));self.assertEqual(self.health(0)[0].version,b'peer');self.assertEqual(self.health(0)[1],100)
 def test_status_rejects_malformed_without_replacing_last_good(self):
  self.connect();self.feed(0,self.status_frame());bad=self.lib.link_bad(0)
  for packet in [self.status_frame(identity=2,flags=4),self.status_frame(identity=2,schema=2),self.status_frame(identity=0),self.status_frame(identity=2,version=b''),self.status_frame(identity=2,version=b'x'*32),self.status_frame(identity=2,version=b'<script>\n'),self.status_frame(identity=2,version=b'a\0b'),frame(5,2,22,2,b'short')]:self.feed(0,packet)
  self.assertEqual(self.lib.link_bad(0),bad+8);self.assertEqual(self.health(0)[0].version,b'peer')
 def test_status_reset_loss_and_stale_session_never_remain_visible(self):
  self.connect();self.feed(0,self.status_frame(identity=9));self.assertIsNotNone(self.health(0));self.lib.link_tick(0,3500);self.assertIsNone(self.health(0));self.feed(0,frame(2,2,22,payload=struct.pack('<QI',11,3)));self.feed(0,self.status_frame(identity=8));self.assertIsNone(self.health(0));self.feed(0,self.status_frame(identity=10));self.assertIsNotNone(self.health(0));self.feed(0,frame(1,2,33,payload=struct.pack('<I',3)));self.assertIsNone(self.health(0));self.feed(0,frame(2,2,33,payload=struct.pack('<QI',11,3)));self.feed(0,self.status_frame(boot=22,identity=11));self.assertIsNone(self.health(0));self.feed(0,self.status_frame(boot=33,identity=1));self.assertIsNotNone(self.health(0))
 def test_status_legacy_peer_keeps_echo_and_cannot_receive_status(self):
  self.feed(0,frame(2,2,22,payload=struct.pack('<QI',11,1)));self.assertTrue(self.lib.link_ready(0));self.assertFalse(self.send_health());self.feed(0,self.status_frame());self.assertIsNone(self.health(0));i=self.lib.link_ping(0,b'x',1,1000);self.assertTrue(i);self.feed(0,frame(4,2,22,i,b'x'));self.assertEqual(self.event().result,0)
 def test_status_quota_sequence_exhaustion_and_invalid_local_input(self):
  self.connect()
  for _ in range(8):self.assertTrue(self.send_health())
  self.assertFalse(self.send_health());self.pump();self.assertIsNotNone(self.health(1));self.assertTrue(self.send_health());self.lib.link_health_serial(0,0xffffffff);self.assertFalse(self.send_health());h=Health(version=b'bad version');self.assertFalse(self.lib.link_health_send(1,C.byref(h)));h=Health(version=b'x'*32);self.assertFalse(self.lib.link_health_send(1,C.byref(h)))
