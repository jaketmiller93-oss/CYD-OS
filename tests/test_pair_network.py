"""Actual asynchronous proxy and C5 broker, with nonblocking radio/TLS doubles."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class PairNetwork(unittest.TestCase):
 def test_network_proxy_lifetimes_and_peer_resets(self):
  if not os.getenv('IDF_PATH'):self.skipTest('ESP-IDF cJSON required')
  cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
  with tempfile.TemporaryDirectory()as tmp:
   out=Path(tmp)/'network';includes=[ROOT/'tests/pair_sdk',ROOT/'components/paired_system/include',ROOT/'components/app_services/include',ROOT/'components/device_link/include',cj]
   sources=[ROOT/'tests/pair_network_host.c',ROOT/'components/paired_system/json.c',ROOT/'components/cyd_common/json.c',cj/'cJSON.c']
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all']+['-I'+str(p)for p in includes]+[str(p)for p in sources]+['-lm','-o',str(out)],check=True)
   subprocess.run([str(out)],check=True)
