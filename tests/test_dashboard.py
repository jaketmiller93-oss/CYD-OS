"""Browser smoke test for the real embedded dashboard using a simulated device API."""
import json
from pathlib import Path
import threading
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Dashboard(unittest.TestCase):
 def test_ipad_folders_clock_and_verified_os_restart(self):
  try:from playwright.sync_api import sync_playwright
  except ImportError:self.skipTest('Playwright is optional')
  catalog={'folders':[{'id':'1234abcd','name':'Tools'}],'projects':[{'package_id':'org.example.hello','name':'Hello','folder':'','versions':[{'revision':'a'*16,'version':'1.0'},{'revision':'b'*16,'version':'1.1'}]}]};calls=[]
  os_state={'state':'idle','boot_confirmed':True,'pending_verify':False,'rollback_available':False,'version':'0.3.0','board':'nm_cyd_c5','running_slot':'ota_0','message':'Startup confirmed','max_image_bytes':0x700000}
  device={'brightness':100,'rotation':0,'utc_offset_minutes':0,'clock':'2026-11-14 12:00','display_mode':0,'seconds_left':0,'message':'Ready'}
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_GET(self):self.handle_api()
   def do_POST(self):self.handle_api()
   def handle_api(self):
    if self.path=='/':
     self.send_response(200);self.send_header('Content-Type','text/html');self.end_headers();self.wfile.write((ROOT/'components/web_api/dashboard.html').read_bytes());return
    if self.headers.get('Authorization')!='Bearer test-access-token':self.send_response(401);self.end_headers();self.wfile.write(b'{"error":"Authentication required"}');return
    raw=self.rfile.read(int(self.headers.get('Content-Length','0')));body=json.loads(raw) if raw and self.headers.get('Content-Type')=='application/json' else None;calls.append((self.command,self.path,body))
    result={}
    if self.path=='/api/status':result={'firmware':os_state['version'],'ip':'192.168.4.1'}
    if self.path=='/api/projects':
     if body and body['action']=='move':catalog['projects'][0]['folder']=body['folder']
     result=catalog
    if self.path=='/api/device':
     if body and body['action']=='rotation':device.update(display_mode=1,seconds_left=15)
     if body and body['action']=='confirm':device.update(display_mode=0,seconds_left=0)
     result=device
    if self.path=='/api/os/update':os_state.update(state='ready',candidate_version='0.3.1',message='Firmware verified')
    if self.path=='/api/os/reboot':os_state.update(state='idle',version='0.3.1',candidate_version='',message='Startup confirmed')
    if self.path.startswith('/api/os'):result=os_state
    self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(json.dumps(result).encode())
  server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
  try:
   with sync_playwright() as p:
    import shutil
    binary=shutil.which('chromium')
    if not binary:self.skipTest('System Chromium is optional')
    browser=p.chromium.launch(executable_path=binary,headless=True,args=['--no-sandbox']);page=browser.new_page(viewport={'width':820,'height':1180},is_mobile=True,has_touch=True);errors=[];page.on('pageerror',lambda e:errors.append(str(e)));page.on('dialog',lambda d:d.accept());page.goto(f'http://127.0.0.1:{server.server_port}');page.locator('#token').fill('test-access-token');page.locator('#connect').click();page.locator('#catalog summary').wait_for();self.assertEqual(page.locator('#catalog summary').count(),1)
    page.locator('#catalog summary').click();self.assertEqual(page.get_by_role('button',name='Run',exact=True).count(),2);page.locator('#catalog select').select_option('1234abcd');page.get_by_role('button',name='Move',exact=True).click();page.get_by_role('button',name='▣ Tools').click();page.locator('#catalog summary').wait_for();self.assertEqual(page.locator('#browse-title').inner_text(),'Tools')
    page.locator('#tab-settings').click();page.locator('#sync-clock').click();page.wait_for_timeout(250);self.assertTrue(any(c[1]=='/api/device' and c[2] and c[2].get('action')=='clock' for c in calls))
    page.locator('#rotation').select_option('1');page.locator('#set-rotation').click();page.locator('#confirm-display').wait_for(state='visible');page.locator('#confirm-display').click()
    page.locator('#tab-system').click();page.locator('#firmware').set_input_files({'name':'cyd-os.bin','mimeType':'application/octet-stream','buffer':b'\xe9'+bytes(1023)});page.locator('#upload-os').click();page.locator('#restart-os').wait_for(state='visible');self.assertIn('Firmware verified',page.locator('#os-status').inner_text());page.locator('#restart-os').click();page.wait_for_function("document.querySelector('#message').textContent.includes('startup confirmed')");self.assertIn('0.3.1',page.locator('#message').inner_text());self.assertEqual(errors,[])
    # The page remains usable on a small phone-size viewport as well.
    page.set_viewport_size({'width':375,'height':812});self.assertTrue(page.evaluate('document.documentElement.scrollWidth <= innerWidth'))
    browser.close()
  finally:server.shutdown();server.server_close()

 def test_remember_access_new_visits_forget_opt_out_and_revoked_token(self):
  try:from playwright.sync_api import sync_playwright
  except ImportError:self.skipTest('Playwright is optional')
  import shutil
  binary=shutil.which('chromium')
  if not binary:self.skipTest('System Chromium is optional')
  current_token=['test-access-token'];offline=[False]
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_GET(self):
    if self.path=='/':
     self.send_response(200);self.send_header('Content-Type','text/html');self.end_headers();self.wfile.write((ROOT/'components/web_api/dashboard.html').read_bytes());return
    if offline[0]:code,data=503,{'error':'Temporarily unavailable'}
    elif self.headers.get('Authorization')!='Bearer '+current_token[0]:code,data=401,{'error':'Authentication required'}
    else:
     code=200
     data={'/api/status':{'firmware':'0.3.2','ip':'192.168.4.1'},'/api/projects':{'folders':[],'projects':[]},'/api/os':{'state':'idle','boot_confirmed':True,'version':'0.3.2','board':'nm_cyd_c5','running_slot':'ota_0','message':'Startup confirmed','max_image_bytes':0x700000},'/api/device':{'clock':'2026-11-14 12:00','utc_offset_minutes':0,'display_mode':0,'seconds_left':0,'message':'Ready'}}.get(self.path,{})
    self.send_response(code);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(json.dumps(data).encode())
  server=ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
  try:
   with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=binary,headless=True,args=['--no-sandbox']);context=browser.new_context();page=context.new_page();url=f'http://127.0.0.1:{server.server_port}'
    def connected(tab):tab.wait_for_function("document.querySelector('#message').textContent === 'Connected.'")
    page.goto(url);self.assertTrue(page.locator('#remember').is_checked());page.locator('#token').fill(current_token[0]);page.locator('#connect').click();connected(page)
    self.assertEqual(page.evaluate("localStorage.getItem('cyd-token')"),current_token[0]);self.assertEqual(page.locator('#token').input_value(),'')
    # A fresh browser session retains local storage but has no sessionStorage.
    state=context.storage_state();context.close();context=browser.new_context(storage_state=state);page=context.new_page();page.goto(url);connected(page);self.assertTrue(page.locator('#login').is_hidden())
    # A connection/service failure must not discard a working saved credential.
    offline[0]=True;page.reload();page.wait_for_function("document.querySelector('#message').textContent.includes('Temporarily unavailable')");self.assertEqual(page.evaluate("localStorage.getItem('cyd-token')"),current_token[0]);offline[0]=False;page.reload();connected(page)
    page.locator('#tab-settings').click();page.locator('#disconnect').click();self.assertTrue(page.locator('#login').is_visible());self.assertIsNone(page.evaluate("localStorage.getItem('cyd-token')"));self.assertIsNone(page.evaluate("sessionStorage.getItem('cyd-session-token')"))
    # Opting out remains limited to this tab, including after a reload.
    page.locator('#remember').uncheck();page.locator('#token').fill(current_token[0]);page.locator('#connect').click();connected(page);page.reload();connected(page);self.assertIsNone(page.evaluate("localStorage.getItem('cyd-token')"));other=context.new_page();other.goto(url);self.assertTrue(other.locator('#login').is_visible());self.assertTrue(other.locator('#workspace').is_hidden());other.close()
    page.locator('#tab-settings').click();page.locator('#disconnect').click();page.locator('#remember').check();page.locator('#token').fill(current_token[0]);page.locator('#connect').click();connected(page)
    current_token[0]='replacement-device-token';page.reload();page.wait_for_function("document.querySelector('#message').textContent.includes('not accepted')");self.assertTrue(page.locator('#login').is_visible());self.assertIsNone(page.evaluate("localStorage.getItem('cyd-token')"))
    # Old tab-only saves migrate without retyping during this update.
    page.evaluate("sessionStorage.setItem('cyd-token','replacement-device-token')");page.reload();connected(page);self.assertEqual(page.evaluate("localStorage.getItem('cyd-token')"),current_token[0])
    context.close();context=browser.new_context();context.add_init_script("Object.defineProperty(window,'localStorage',{get(){throw new Error('Storage disabled')}});Object.defineProperty(window,'sessionStorage',{get(){throw new Error('Storage disabled')}});");page=context.new_page();page.goto(url);page.locator('#token').fill(current_token[0]);page.locator('#connect').click();page.wait_for_function("document.querySelector('#message').textContent.includes('could not save')");self.assertTrue(page.locator('#login').is_hidden());browser.close()
  finally:server.shutdown();server.server_close()
