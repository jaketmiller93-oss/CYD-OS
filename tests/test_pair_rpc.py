"""Actual protocol: simultaneous service calls, reset isolation and forged types."""
import ctypes as C
from pathlib import Path
import subprocess,tempfile,unittest
from test_link import Link,Event,frame
ROOT=Path(__file__).resolve().parents[1]
class PairRPC(Link):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();out=Path(cls.tmp.name)
  extra=out/'rpc.c';extra.write_text('''#include "cyd_link.h"
extern cyd_link_t nodes[2];
uint32_t request(unsigned x,const void *p,size_t n,unsigned t){return cyd_link_request(&nodes[x],p,n,t);}
int receive(unsigned x,cyd_link_event_t *p){return cyd_link_receive(&nodes[x],p);}
int reply(unsigned x,uint64_t boot,uint32_t id,const void *p,size_t n){return cyd_link_reply(&nodes[x],boot,id,p,n);}
''')
  # A separate copy keeps the older protocol tests' no-RPC build intact.
  harness=out/'harness.c';harness.write_text((ROOT/'tests/link_harness.c').read_text().replace('static cyd_link_t nodes[2]','cyd_link_t nodes[2]'))
  sources=[str(ROOT/'components/device_link/link.c'),str(harness),str(extra)]
  common=['cc','-Wall','-Wextra','-Werror','-DCYD_LINK_ENABLE_RPC=1','-I'+str(ROOT/'components/device_link/include')]
  subprocess.run(common+['-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all']+sources+['-o',str(out/'link.so')],check=True)
  cls.lib=C.CDLL(str(out/'link.so'));l=cls.lib
  l.link_reset.argtypes=[C.c_uint,C.c_int,C.c_uint64];l.link_tick.argtypes=[C.c_uint,C.c_uint64];l.link_feed.argtypes=[C.c_uint,C.c_void_p,C.c_size_t];l.link_tx.argtypes=[C.c_uint,C.c_void_p,C.c_size_t];l.link_tx.restype=C.c_size_t;l.link_ping.argtypes=[C.c_uint,C.c_void_p,C.c_size_t,C.c_uint];l.link_ping.restype=C.c_uint32;l.link_event.argtypes=[C.c_uint,C.POINTER(Event)]
  l.request.argtypes=[C.c_uint,C.c_void_p,C.c_size_t,C.c_uint];l.request.restype=C.c_uint32;l.receive.argtypes=[C.c_uint,C.POINTER(Event)];l.reply.argtypes=[C.c_uint,C.c_uint64,C.c_uint32,C.c_void_p,C.c_size_t]
 def test_service_duplex_and_type_isolation(self):
  self.connect(1)
  a=self.lib.request(0,b'host file',9,1000);b=self.lib.request(1,b'manager',7,1000);self.pump(2)
  e=Event();self.assertTrue(self.lib.receive(1,C.byref(e)));self.assertEqual(bytes(e.data[:e.size]),b'host file')
  # A valid old echo with the same ID must not complete a service call.
  self.feed(0,frame(4,2,22,a,b'forged'));self.assertIsNone(self.event(0))
  self.assertTrue(self.lib.reply(1,11,a,b'result',6));self.assertFalse(self.lib.reply(0,999,b,b'bad',3))
  self.assertTrue(self.lib.receive(0,C.byref(e)));self.assertTrue(self.lib.reply(0,22,b,b'other',5));self.pump(1)
  self.assertEqual(bytes(self.event(0).data[:6]),b'result');self.assertEqual(bytes(self.event(1).data[:5]),b'other')
 def test_service_request_limits_and_reset(self):
  self.connect();self.assertFalse(self.lib.request(0,b'x'*249,249,1000));self.assertFalse(self.lib.request(0,b'x',1,30001));large=self.lib.request(0,b'x',1,30000);self.assertTrue(large);self.lib.link_cancel(0,large)
  identity=self.lib.request(0,b'write',5,1000);self.pump();e=Event();self.assertTrue(self.lib.receive(1,C.byref(e)))
  self.lib.link_reset(1,2,33);self.lib.link_tick(1,10);self.pump();self.assertEqual(self.event().result,3)
  self.assertFalse(self.lib.reply(1,11,identity,b'stale',5))
 # Run only the service tests here; Link already owns its legacy suite.
for name in list(vars(Link)):
 if name.startswith('test_'):setattr(PairRPC,name,None)

del Link
