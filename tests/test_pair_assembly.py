"""Real RPC client/server assembly with a synchronous transport double."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class PairAssembly(unittest.TestCase):
 def test_client_server_bounds_sessions_and_uncertain_mutation(self):
  if not os.getenv('IDF_PATH'):self.skipTest('ESP-IDF cJSON is required')
  cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
  with tempfile.TemporaryDirectory() as tmp:
   out=Path(tmp)/'assembly'
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tests/pair_sdk'),'-I'+str(ROOT/'components/paired_system/include'),'-I'+str(ROOT/'components/ota_manager/include'),'-I'+str(ROOT/'components/device_link/include'),'-I'+str(ROOT/'components/device_settings/include'),'-I'+str(ROOT/'components/app_services/include'),'-I'+str(cj),str(ROOT/'tests/pair_rpc_host.c'),str(ROOT/'components/cyd_common/json.c'),str(cj/'cJSON.c'),'-o',str(out)],check=True)
   subprocess.run([str(out)],check=True)
