"""Production protocol engine against real local TCP/TLS peers (OpenSSL host adapter).
This does not test ESP-TLS, lwIP DNS, RF, or physical hardware.
"""
import ctypes as C
from pathlib import Path
import socket,ssl,threading,time,subprocess,tempfile,unittest
from test_services import Event,ROOT
class Loopback(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();d=Path(cls.tmp.name);out=d/'network.so'
  subprocess.run(['cc','-shared','-fPIC','-DCYD_HOST_NETWORK','-I'+str(ROOT/'components/app_services/include'),str(ROOT/'components/app_services/services.c'),str(ROOT/'tests/services_harness.c'),str(ROOT/'tests/host_network.c'),'-lssl','-lcrypto','-o',str(out)],check=True)
  cls.lib=C.CDLL(str(out));l=cls.lib;l.test_network_http.argtypes=[C.c_char_p,C.c_char_p,C.c_char_p,C.c_int,C.c_void_p,C.c_size_t,C.c_uint];l.test_network_mqtt.argtypes=[C.c_char_p,C.c_char_p,C.c_char_p];l.test_service_tick.argtypes=[C.c_ulonglong];l.test_service_pop.argtypes=[C.POINTER(Event)];l.test_service_subscribe.argtypes=[C.c_uint,C.c_uint,C.c_char_p]
  cls.cert=d/'cert.pem';cls.key=d/'key.pem';subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-keyout',str(cls.key),'-out',str(cls.cert),'-subj','/CN=localhost','-addext','subjectAltName=DNS:localhost','-days','1'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);cls.ca=cls.cert.read_bytes()
 @classmethod
 def tearDownClass(cls):cls.lib.test_service_close();cls.tmp.cleanup()
 def setUp(self):
  for peer in getattr(self,"peers",[]):peer.close()
  self.lib.test_network_reset();self.start=time.monotonic();self.peers=[];self.threads=[];self.server_errors=[]
 def tearDown(self):
  self.lib.test_service_close();[p.close() for p in self.peers]
  for thread in self.threads:thread.join(2.2)
  self.assertEqual(self.server_errors,[])
 def server(self,fn,tls=False):
  listener=socket.socket();listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(2);self.peers.append(listener);port=listener.getsockname()[1]
  def run():
   try:
    c,_=listener.accept();c.settimeout(2)
    with c:
     if tls:
      ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(self.cert,self.key)
      with ctx.wrap_socket(c,server_side=True) as secure:fn(secure)
     else:fn(c)
   except (OSError,ssl.SSLError):pass
   except Exception as e:self.server_errors.append(str(e))
  thread=threading.Thread(target=run,daemon=True);self.threads.append(thread);thread.start();return port
 def tick(self):self.lib.test_service_tick(int((time.monotonic()-self.start)*1000))
 def event(self,timeout=3):
  end=time.monotonic()+timeout
  while time.monotonic()<end:
   self.tick();e=Event()
   if self.lib.test_service_pop(C.byref(e)):return e
   time.sleep(.002)
  self.fail('No bounded completion event')
 def request(self,url,ca=b'',name=b'',body=b'',timeout=1000):self.assertGreater(self.lib.test_network_http(url.encode(),ca,name,int(bool(body)),body,len(body),timeout),0);return self.event()
 def test_real_http_post_status_and_fragmented_response(self):
  received=[]
  def peer(c):
   request=b''
   while b'\r\n\r\n' not in request:request+=c.recv(2048)
   head,body=request.split(b'\r\n\r\n',1)
   while len(body)<7:
    chunk=c.recv(7-len(body));self.assertTrue(chunk);body+=chunk
   received.append((head,body));response=b'HTTP/1.1 201 Created\r\nContent-Length: 5\r\n\r\nhello'
   for i in range(0,len(response),2):c.sendall(response[i:i+2]);time.sleep(.002)
  port=self.server(peer);e=self.request(f'http://127.0.0.1:{port}/test',body=b'{"x":1}');self.assertEqual(e.status,201);self.assertEqual(bytes(e.body[:e.size]),b'hello');self.assertIn(b'POST /test',received[0][0]);self.assertEqual(received[0][1],b'{"x":1}')
 def test_verified_private_https_and_wrong_hostname(self):
  def peer(c):c.recv(2048);c.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok')
  port=self.server(peer,True);e=self.request(f'https://127.0.0.1:{port}/',self.ca,b'localhost');self.assertEqual(e.status,200)
  self.setUp();port=self.server(peer,True);e=self.request(f'https://127.0.0.1:{port}/',self.ca,b'wrong.example');self.assertEqual(e.status,-1);self.assertIn(b'TLS',e.error)
 def test_untrusted_https_never_falls_back_to_plaintext(self):
  seen=[]
  def peer(c):seen.append(c.recv(64))
  port=self.server(peer);e=self.request(f'https://127.0.0.1:{port}/',timeout=200);self.assertEqual(e.status,-1);self.assertTrue(seen);self.assertNotIn(b'GET ',seen[0])
 def test_slow_peer_deadline_and_cancel_cleanup(self):
  released=threading.Event()
  def peer(c):
   c.recv(2048)
   try:
    while True:c.sendall(b'H');time.sleep(.02)
   except OSError:released.set()
  port=self.server(peer);e=self.request(f'http://127.0.0.1:{port}/',timeout=120);self.assertEqual(e.status,-1);self.assertIn(b'deadline',e.error);self.assertTrue(released.wait(1))
  self.setUp();port=self.server(peer);gen=self.lib.test_network_http(f'http://127.0.0.1:{port}/'.encode(),b'',b'',0,b'',0,5000);self.tick();self.assertTrue(self.lib.test_service_cancel(0,gen));self.assertIsNone(self.lib.test_service_pop(C.byref(Event())) or None)
 def test_real_mqtt_tls_connect_subscribe_and_fragmented_binary_message(self):
  seen=[]
  def packet(c):
   first=c.recv(1);self.assertTrue(first);remaining=0;mult=1
   while True:
    b=c.recv(1)[0];remaining+=(b&127)*mult
    if b<128:break
    mult*=128
   data=b''
   while len(data)<remaining:data+=c.recv(remaining-len(data))
   return first[0],data
  def peer(c):
   kind,data=packet(c);seen.append(data);self.assertEqual(kind,16);c.sendall(b'\x20\x02\0\0');kind,data=packet(c);self.assertEqual(kind,0x82);c.sendall(b'\x90\x03'+data[:2]+b'\0');topic=b'status/value';body=b'ok\0binary';payload=len(topic).to_bytes(2,'big')+topic+body;frame=bytes([0x30,len(payload)])+payload
   for b in frame:c.sendall(bytes([b]));time.sleep(.002)
   time.sleep(.1)
  for tls in [False,True]:
   self.lib.test_network_reset();self.start=time.monotonic();port=self.server(peer,tls);scheme='mqtts' if tls else 'mqtt';gen=self.lib.test_network_mqtt(f'{scheme}://127.0.0.1:{port}'.encode(),self.ca if tls else b'',b'localhost' if tls else b'');self.assertGreater(gen,0);self.lib.test_service_subscribe(1,gen,b'status/#');e=self.event();self.assertEqual(e.kind,2);self.assertEqual(e.status,1);e=self.event();self.assertEqual(e.kind,1);self.assertEqual(bytes(e.body[:e.size]),b'ok\0binary');self.assertIn(b'user',seen[-1]);self.assertIn(b'password',seen[-1])
