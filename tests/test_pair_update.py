"""Real coordinator/file hashes/policy with SDK flash and peer doubles."""
from pathlib import Path
import hashlib,os,struct,subprocess,tempfile,unittest,zlib
ROOT=Path(__file__).resolve().parents[1]
def image(board,chip,role,letter):
 b=bytearray(1024);b[0]=0xe9;b[1]=1;b[3]=0x40;b[23]=1;struct.pack_into('<H',b,12,chip);struct.pack_into('<II',b,28,448,0xabcd5432);b[48:54]=b'0.5.0\0';b[80:87]=b'cyd-os\0';b[176:208]=bytes([letter])*32
 struct.pack_into('<16sI24sIIIII65s63s',b,288,b'CYD-OS-UPDATE',1,board.encode(),15,1,1,1,1,b'0'*64,bytes([1,role])+bytes(61));return b
class PairUpdate(unittest.TestCase):
 def test_coordinator_success_and_failure_boundaries(self):
  if not os.getenv('IDF_PATH'):self.skipTest('ESP-IDF cJSON required')
  cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
  with tempfile.TemporaryDirectory()as tmp:
   root=Path(tmp);data=[image('crowpanel_s3',9,1,83),image('nm_cyd_c5',0x17,2,67)];header=bytearray(512)
   struct.pack_into('<16sIIII32s',header,0,b'CYD-PAIR-UPDATE\0',1,2,2560,0,b'0.5.0');offset=512
   for i,(board,b)in enumerate(zip(['crowpanel_s3','nm_cyd_c5'],data)):struct.pack_into('<24s32sII32s',header,64+128*i,board.encode(),b'0.5.0',len(b),offset,hashlib.sha256(b).digest());offset+=len(b)
   struct.pack_into('<I',header,508,zlib.crc32(header[:508]));bundle=root/'test.cydos';bundle.write_bytes(header+b''.join(data));out=root/'update'
   includes=[ROOT/'tests/pair_sdk',ROOT/'tests/pair_crypto',ROOT/'components/paired_system/include',ROOT/'components/ota_manager/include',ROOT/'components/device_link/include',cj]
   sources=[ROOT/'tests/pair_update_host.c',ROOT/'components/paired_system/bundle.c',ROOT/'components/paired_system/json.c',ROOT/'components/ota_manager/policy.c',ROOT/'components/cyd_common/json.c',cj/'cJSON.c']
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DTEST_PAIR_HOST=1','-DCYD_ROOT="'+str(root/'drive')+'"']+['-I'+str(p)for p in includes]+[str(p)for p in sources]+['-lm','-lcrypto','-o',str(out)],check=True)
   for scenario in range(6):
    r=subprocess.run([str(out),str(scenario),str(bundle)],capture_output=True,text=True);self.assertEqual(r.returncode,0,str(scenario)+': '+r.stdout+r.stderr);print(r.stdout.strip())
