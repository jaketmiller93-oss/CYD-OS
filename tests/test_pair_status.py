"""A single dashboard must not report the manager's idle runtime as the app host."""
import os,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class PairStatus(unittest.TestCase):
 def test_primary_runtime_storage_and_unavailable_host(self):
  if not os.getenv('IDF_PATH'):self.skipTest('ESP-IDF cJSON required')
  cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
  with tempfile.TemporaryDirectory()as tmp:
   out=Path(tmp)/'status';includes=[ROOT/'tests/pair_sdk',ROOT/'components/paired_system/include',cj];sources=[ROOT/'tests/pair_status_host.c',ROOT/'components/paired_system/json.c',cj/'cJSON.c']
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all']+['-I'+str(p)for p in includes]+[str(p)for p in sources]+['-lm','-o',str(out)],check=True);subprocess.run([str(out)],check=True)
