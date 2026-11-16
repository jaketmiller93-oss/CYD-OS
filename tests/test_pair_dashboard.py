"""Browser confirmation must cover the paired transition, not the old boot."""
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json,shutil,threading,unittest
ROOT=Path(__file__).resolve().parents[1]
class PairDashboard(unittest.TestCase):
 def test_paired_progress_fallback_and_offline_manager(self):
  try:from playwright.sync_api import sync_playwright
  except ImportError:self.skipTest('Playwright optional')
  binary=shutil.which('chromium')
  if not binary:self.skipTest('Chromium optional')
  state={'phase':'idle','version':'0.5.0','host':True,'candidate_elf':'1'*64,'candidate':'0.5.1','elf_override':None};settings=[]
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def reply(self,data,code=200):self.send_response(code);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(json.dumps(data).encode())
   def do_GET(self):
    if self.path=='/':self.send_response(200);self.send_header('Content-Type','text/html');self.end_headers();self.wfile.write((ROOT/'components/web_api/dashboard.html').read_bytes());return
    if self.headers.get('Authorization')!='Bearer fixture-manager-token':self.reply({'error':'Authentication required'},401);return
    if self.path=='/api/status':self.reply({'firmware':'0.5.0','ip':'192.168.4.1','paired_system':True})
    elif self.path=='/api/projects':self.reply({'folders':[],'projects':[]}if state['host']else{'error':'S3 unavailable'},200 if state['host']else 503)
    elif self.path=='/api/os':self.reply({'state':state['phase']if state['host']else'unavailable','version':state['version'],'candidate_version':state['candidate'],'candidate_elf_sha256':state['candidate_elf'],'elf_sha256':state['elf_override']or('1'*64 if state['version']=='0.5.1'else'0'*64),'board':'paired_tablet','running_slot':'S3 + C5','boot_confirmed':state['host'],'pending_verify':False,'max_image_bytes':0xe00200,'message':'S3 coordinator '+state['phase']})
    elif self.path=='/api/device':self.reply({'clock':'2026-11-15 10:00','utc_offset_minutes':0,'display_mode':0,'seconds_left':0,'message':'C5 settings ready'})
    else:self.reply({})
   def do_POST(self):
    data=self.rfile.read(int(self.headers.get('Content-Length','0')))
    if self.headers.get('Authorization')!='Bearer fixture-manager-token':self.reply({},401);return
    if self.path=='/api/os/update':state['phase']='verifying';self.reply({'state':'verifying','version':state['version'],'board':'paired_tablet','boot_confirmed':True,'max_image_bytes':0xe00200,'message':'Verifying both images'},201)
    elif self.path=='/api/os/reboot':state['phase']='uploading';self.reply({})
    elif self.path=='/api/device':settings.append(json.loads(data));self.reply({'ok':True})
    else:self.reply({})
  server=ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
  try:
   with sync_playwright()as p:
    browser=p.chromium.launch(executable_path=binary,headless=True,args=['--no-sandbox']);page=browser.new_page();errors=[];page.on('pageerror',lambda e:errors.append(str(e)));page.on('dialog',lambda d:d.accept());page.goto(f'http://127.0.0.1:{server.server_port}');page.locator('#token').fill('fixture-manager-token');page.locator('#connect').click();page.wait_for_function("document.querySelector('#message').textContent==='Connected.'")
    page.locator('#tab-system').click();self.assertIn('.cydos',page.locator('label[for=firmware]').inner_text());page.locator('#firmware').set_input_files({'name':'system.cydos','mimeType':'application/octet-stream','buffer':bytes(2048)});page.locator('#upload-os').click();page.wait_for_function("document.querySelector('#message').textContent.includes('Wait for both images')");self.assertTrue(page.locator('#restart-os').is_hidden())
    state['phase']='ready';page.evaluate('poll()');page.locator('#restart-os').wait_for(state='visible');page.locator('#restart-os').click();page.wait_for_function("document.querySelector('#message').textContent.includes('Restart requested')");page.evaluate('poll()');self.assertTrue(page.evaluate('restarting'));self.assertNotIn('startup confirmed',page.locator('#message').inner_text())
    state.update(phase='idle',version='0.5.1');page.evaluate('poll()');self.assertFalse(page.evaluate('restarting'));self.assertIn('0.5.1; startup confirmed',page.locator('#message').inner_text())
    state.update(phase='ready',version='0.5.0');page.evaluate('poll()');page.locator('#restart-os').click();page.wait_for_function('restarting');state['phase']='idle';page.evaluate('poll()');self.assertFalse(page.evaluate('restarting'));self.assertIn('incomplete',page.locator('#message').inner_text())
    state.update(phase='ready',version='0.5.1',candidate='0.5.1',candidate_elf='2'*64,elf_override='1'*64);page.evaluate('poll()');page.locator('#restart-os').click();page.wait_for_function('restarting');state['phase']='idle';page.evaluate('poll()');self.assertFalse(page.evaluate('restarting'));self.assertIn('incomplete',page.locator('#message').inner_text())
    state.update(host=False,phase='idle');page.reload();page.wait_for_function("document.querySelector('#message').textContent.includes('C5 settings remain accessible')");self.assertTrue(page.locator('#login').is_hidden());page.locator('#tab-settings').click();page.locator('#sync-clock').click();page.wait_for_function("document.querySelector('#message').textContent.includes('Setting requested')");self.assertEqual(settings[-1]['action'],'clock');self.assertEqual(errors,[]);browser.close()
  finally:server.shutdown();server.server_close()
