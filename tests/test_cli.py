import contextlib
import io
import json
import os
from pathlib import Path
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/cydctl'))
import cydctl
TOKEN='test-token-that-is-long-enough'
REVISION='0123456789abcdef'

class Device(BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def reply(self,data,status=200):
        self.send_response(status);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(json.dumps(data).encode())
    def authorized(self):
        if self.headers.get('Authorization')!='Bearer '+TOKEN:
            self.reply({'error':'Authentication required'},401);return False
        return True
    def do_GET(self):
        if not self.authorized():return
        if self.path=='/api/status':self.reply({'target':self.server.target,'app':{'state':'crashed' if self.server.crash else 'running','revision':REVISION,'error':'Script error' if self.server.crash else ''}})
        elif self.path=='/api/logs':self.reply(['hello'])
        elif self.path=='/api/versions':self.reply([{'revision':REVISION}])
        else:self.reply({'error':'Missing'},404)
    def do_POST(self):
        if not self.authorized():return
        data=self.rfile.read(int(self.headers.get('Content-Length',0)))
        if self.path=='/api/packages':
            self.server.package=data;self.reply({'revision':REVISION,'status':'installed'},201)
        elif self.path.startswith('/api/run?'):
            self.server.runs+=1;self.reply({'status':'starting'},202)
        elif self.path=='/api/stop':self.reply({'state':'stopped'})
        else:self.reply({'error':'Missing'},404)

class Cli(unittest.TestCase):
    def setUp(self):
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Device)
        self.server.package=None;self.server.runs=0;self.server.crash=False;self.server.target="esp32s3"
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
        self.url=f'http://127.0.0.1:{self.server.server_port}'
    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join()
    def call(self,*args,token=TOKEN):
        output=io.StringIO()
        with patch.dict(os.environ,{'CYD_TOKEN':token}),contextlib.redirect_stdout(output):
            status=cydctl.main([*args,'--device',self.url,'--json'])
        return status,json.loads(output.getvalue())
    def test_deploy_run_and_logs(self):
        code,data=self.call('deploy',str(ROOT/'apps/hello'),'--run','--logs')
        self.assertEqual(code,0);self.assertEqual(data['app']['state'],'running');self.assertEqual(data['logs'],['hello'])
        self.assertEqual(self.server.runs,1);self.assertTrue(self.server.package.startswith(b'PK\x03\x04'))
    def test_same_package_deploys_to_c5(self):
        self.server.target='esp32c5'
        code,data=self.call('deploy',str(ROOT/'apps/hello'),'--run')
        self.assertEqual(code,0);self.assertEqual(data['app']['state'],'running')
    def test_incompatible_target_is_rejected_before_upload(self):
        self.server.target='esp32c6'
        code,data=self.call('deploy',str(ROOT/'apps/hello'),'--run')
        self.assertEqual(code,1);self.assertIsNone(self.server.package)
        self.assertIn('does not support',data['error'])
    def test_auth_failure_is_machine_readable(self):
        code,data=self.call('status',token='wrong')
        self.assertEqual(code,1);self.assertEqual(data['error'],'Authentication required');self.assertNotIn(TOKEN,json.dumps(data))
    def test_crash_is_not_reported_as_success(self):
        self.server.crash=True
        code,data=self.call('deploy',str(ROOT/'apps/hello'),'--run')
        self.assertEqual(code,1);self.assertIn('Script error',data['error'])
    def test_upload_does_not_imply_run(self):
        code,data=self.call('deploy',str(ROOT/'apps/hello'))
        self.assertEqual(code,0);self.assertEqual(self.server.runs,0)
    def test_unsafe_origins_rejected(self):
        for url in ['file:///tmp/device','http://secret@cyd.local','http://cyd.local/a','http://cyd.local?x=y']:
            with self.assertRaises(cydctl.CydError):cydctl.Client(url,TOKEN)
    def test_redirect_refuses_credential_forwarding(self):
        with self.assertRaises(cydctl.CydError):cydctl.NoRedirect().redirect_request(None,None,302,'',{},'http://other.local/')
    def test_non_json_http_errors_include_context_and_redact_token(self):
        from urllib.error import HTTPError
        for body, expected in [(b'Invalid HTTP request', 'Invalid HTTP request'),
                               (b'', 'Empty error response'),
                               (b'Invalid byte: \xff', 'Invalid byte:'),
                               (TOKEN.encode(), '[redacted]')]:
            client=cydctl.Client(self.url,TOKEN)
            error=HTTPError(self.url+'/api/status',400,'Bad Request',{},io.BytesIO(body))
            with patch.object(client.opener,'open',side_effect=error):
                with self.assertRaises(cydctl.CydError) as caught:
                    client.request('status')
            message=str(caught.exception)
            self.assertIn('HTTP 400 (GET /api/status)',message)
            self.assertIn(expected,message)
            self.assertNotIn(TOKEN,message)

if __name__=='__main__':unittest.main()
