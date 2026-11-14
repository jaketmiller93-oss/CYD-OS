"""Actual portable service state machine with deterministic nonblocking IO doubles."""
import ctypes as C
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class Event(C.Structure):
 _fields_=[('kind',C.c_int),('generation',C.c_uint32),('slot',C.c_uint),('status',C.c_int),('size',C.c_size_t),('topic',C.c_char*129),('error',C.c_char*96),('body',C.c_ubyte*8193)]
class Services(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();out=Path(cls.tmp.name)/'services.so'
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'components/app_services/include'),str(ROOT/'components/app_services/services.c'),str(ROOT/'tests/services_harness.c'),'-o',str(out)],check=True)
  cls.lib=C.CDLL(str(out));l=cls.lib;l.test_event_size.restype=C.c_size_t;assert C.sizeof(Event)==l.test_event_size()
  l.test_service_tick.argtypes=[C.c_ulonglong];l.test_service_input.argtypes=[C.c_void_p,C.c_size_t,C.c_uint];l.test_service_pop.argtypes=[C.POINTER(Event)];l.test_service_http.argtypes=[C.c_char_p,C.c_int];l.test_service_subscribe.argtypes=[C.c_uint,C.c_uint,C.c_char_p];l.test_service_publish.argtypes=[C.c_uint,C.c_uint,C.c_char_p,C.c_void_p,C.c_size_t];l.test_service_output.argtypes=[C.c_void_p,C.c_size_t];l.test_service_output.restype=C.c_size_t;l.test_endpoint.argtypes=[C.c_char_p,C.c_char_p,C.c_char_p]
 @classmethod
 def tearDownClass(cls):cls.lib.test_service_close();cls.tmp.cleanup()
 def setUp(self):self.lib.test_service_reset()
 def feed(self,data,chunk=0):self.lib.test_service_input(data,len(data),chunk)
 def ticks(self,start=0,count=20):
  for i in range(count):self.lib.test_service_tick(start+i)
 def pop(self):
  e=Event();return e if self.lib.test_service_pop(C.byref(e)) else None
 def output(self):
  b=C.create_string_buffer(65536);n=self.lib.test_service_output(b,len(b));return b.raw[:n]
 def test_http_fragmented_get_and_status_body(self):
  self.assertGreater(self.lib.test_service_http(b'http://example.test/data',5000),0);self.feed(b'HTTP/1.1 404 Not Found\r\nContent-Length: 5\r\n\r\nhello',1);self.ticks(count=100);e=self.pop();self.assertEqual(e.status,404);self.assertEqual(bytes(e.body[:e.size]),b'hello');self.assertIn(b'GET /data HTTP/1.1',self.output());self.assertEqual(self.lib.test_service_closes(),1)
 def test_chunked_body_and_truncated_chunk(self):
  self.lib.test_service_http(b'http://host/',5000);self.feed(b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n2\r\nde\r\n0\r\n\r\n',1);self.ticks(count=100);e=self.pop();self.assertEqual(bytes(e.body[:e.size]),b'abcde')
  self.setUp();self.lib.test_service_http(b'http://host/',5000);self.feed(b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n8\r\nabc');self.lib.test_service_eof(1);self.ticks();self.assertEqual(self.pop().status,-1)
 def test_http_absolute_deadline_cancellation_and_concurrency(self):
  gen=self.lib.test_service_http(b'http://host/',100);self.assertEqual(self.lib.test_service_http(b'http://host/',100),-1);self.lib.test_service_pending(1);self.ticks(start=100,count=3);self.assertIn(b'deadline',self.pop().error);self.assertEqual(self.lib.test_service_closes(),0)
  gen=self.lib.test_service_http(b'http://host/',5000);self.lib.test_service_tick(200);self.assertTrue(self.lib.test_service_cancel(0,gen));self.assertIsNone(self.pop());self.assertEqual(self.lib.test_service_closes(),1)
 def test_http_rejects_oversized_conflicting_headers_and_bodies(self):
  for headers in [b'Content-Length: 8193\r\n',b'Content-Length: 2\r\nContent-Length: 2\r\n',b'Content-Length: 1\r\nTransfer-Encoding: chunked\r\n']:
   self.setUp();self.lib.test_service_http(b'http://host/',1000);self.feed(b'HTTP/1.1 200 OK\r\n'+headers+b'\r\n');self.ticks();self.assertEqual(self.pop().status,-1)
 def test_url_and_tls_options_are_strict(self):
  for u in [b'ftp://host/',b'https://user:pass@host/',b'http://host:0/',b'http://host/a b',b'http://host/#fragment',b'http://[::1]/']:
   self.assertFalse(self.lib.test_endpoint(u,b'',b''))
  self.assertFalse(self.lib.test_endpoint(b'http://host/',b'CA',b''));self.assertTrue(self.lib.test_endpoint(b'https://host/',b'CA',b'private.example'))
 def test_timer_coalescing_once_cancel_and_quota(self):
  handles=[self.lib.test_service_timer(20,1) for _ in range(16)];self.assertTrue(all(h>=0 for h in handles));self.assertEqual(self.lib.test_service_timer(20,1),-1);self.lib.test_service_tick(20);self.lib.test_service_tick(10000);count=0
  while self.pop():count+=1
  self.assertEqual(count,6);self.assertTrue(self.lib.test_timer_cancel(handles[0]));self.lib.test_service_tick(20000);self.assertNotEqual(self.pop().slot,0)
 def mqtt_ready(self):
  gen=self.lib.test_service_mqtt();self.feed(b'\x20\x02\0\0');self.ticks();self.assertEqual(self.pop().status,1);return gen
 def test_mqtt_fragments_binary_payload_and_outbox_limits(self):
  gen=self.mqtt_ready();self.assertTrue(self.lib.test_service_subscribe(1,gen,b'data/#'));self.ticks(start=50);topic=b'data/x';body=b'hello\0world';p=len(topic).to_bytes(2,'big')+topic+body;self.feed(bytes([0x30,len(p)])+p,1);self.ticks(start=100,count=100);e=self.pop();self.assertEqual(e.topic,topic);self.assertEqual(bytes(e.body[:e.size]),body)
  self.assertTrue(self.lib.test_service_publish(1,gen,b'data/x',b'1',1));self.assertTrue(self.lib.test_service_publish(1,gen,b'data/x',b'2',1));self.assertFalse(self.lib.test_service_publish(1,gen,b'data/x',b'3',1));self.assertFalse(self.lib.test_service_publish(1,gen,b'data/x',b'x'*2049,2049))
 def test_mqtt_reconnect_backoff_restores_subscriptions_and_rejects_stale(self):
  gen=self.mqtt_ready();self.lib.test_service_subscribe(1,gen,b'status/#');self.ticks(start=100);before=self.output().count(b'status/#');self.feed(b'\x30\x06\x00\x01xabc');self.lib.test_service_tick(200);self.lib.test_service_eof(1);self.lib.test_service_tick(201);e=self.pop();self.assertEqual(e.kind,2);self.assertIsNone(self.pop());opens=self.lib.test_service_opens();self.lib.test_service_tick(1200);self.assertEqual(self.lib.test_service_opens(),opens);self.lib.test_service_eof(0);self.feed(b'\x20\x02\0\0');self.ticks(start=1201);self.assertEqual(self.pop().status,1);self.assertGreater(self.output().count(b'status/#'),before);self.assertTrue(self.lib.test_service_publish(1,gen,b'x',b'a',1));self.assertTrue(self.lib.test_service_cancel(1,gen));new=self.lib.test_service_mqtt();self.assertNotEqual(new,gen);self.assertFalse(self.lib.test_service_cancel(1,gen))
 def test_mqtt_incomplete_or_oversized_packets_never_deliver_partial(self):
  self.mqtt_ready();self.feed(b'\x30\xff\x7f');self.ticks(start=50);self.assertEqual(self.pop().kind,2)
  self.setUp();self.mqtt_ready();payload=b'\0\1x'+b'a'*2049;self.feed(b'\x30\x84\x10'+payload);self.ticks(start=50);self.assertEqual(self.pop().kind,2);self.assertIsNone(self.pop())
  self.setUp();self.mqtt_ready();self.feed(b'\x30\x09\x00\x01xabc');self.lib.test_service_tick(100);self.lib.test_service_tick(5200);self.assertEqual(self.pop().kind,2);self.assertIsNone(self.pop())
 def test_close_drops_callbacks_and_closes_all_resources(self):
  self.mqtt_ready();self.assertGreater(self.lib.test_service_mqtt(),0);self.assertEqual(self.lib.test_service_mqtt(),-1);self.lib.test_service_timer(20,1);self.lib.test_service_http(b'http://host/',1000);self.lib.test_service_tick(100);self.lib.test_service_close();self.assertIsNone(self.pop());self.assertEqual(self.lib.test_service_closes(),3)

 def test_timer_fairness_and_reserved_http_completion(self):
  for _ in range(16):self.lib.test_service_timer(20,1)
  seen=set()
  for now in [20,40,60]:
   self.lib.test_service_tick(now)
   while (e:=self.pop()) is not None:seen.add(e.slot)
  self.assertEqual(seen,set(range(16)))
  self.lib.test_service_http(b'http://host/',5000);self.feed(b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n');self.lib.test_service_tick(100);self.lib.test_service_tick(101);events=[]
  while (e:=self.pop()) is not None:events.append(e)
  self.assertTrue(any(e.kind==0 and e.status==200 for e in events))
 def test_rejects_bad_status_and_chunk_framing_without_partial_data(self):
  for raw in [b'HTTP/1.1 999999999999999999999 Bad\r\n\r\n',b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1;bad\r\nx\r\n0\r\n\r\n',b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nxxxx']:
   self.setUp();self.lib.test_service_http(b'http://host/',5000);self.feed(raw);self.ticks();e=self.pop();self.assertEqual(e.status,-1);self.assertEqual(e.size,0)

 def test_mqtt_write_stall_and_connection_rejection_are_bounded(self):
  gen=self.mqtt_ready();self.assertTrue(self.lib.test_service_publish(1,gen,b'topic',b'data',4));self.lib.test_service_stall_write(1);self.lib.test_service_tick(100);self.lib.test_service_tick(5200);e=self.pop();self.assertEqual(e.kind,2);self.assertIn(b'write deadline',e.error)
  self.setUp();self.lib.test_service_mqtt();self.feed(b'\x20\x02\0\x05');self.ticks();e=self.pop();self.assertIn(b'CONNACK rejected',e.error)
