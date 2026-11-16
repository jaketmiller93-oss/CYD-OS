"""Actual two-drive server operations on temporary storage; no SD/device access."""
import ctypes as C
import hashlib,json,os,struct,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LIST,READ,WRITE,DELETE,RENAME,MKDIR,PUT,COMMIT=range(32,40)
class PairFiles(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if not os.getenv('IDF_PATH'):raise unittest.SkipTest('ESP-IDF cJSON required')
  cls.tmp=tempfile.TemporaryDirectory();cls.root=Path(cls.tmp.name)/'drive';cls.root.mkdir();cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON';out=Path(cls.tmp.name)/'files.so'
  inc=[ROOT/'tests/pair_sdk',ROOT/'tests/pair_crypto',ROOT/'components/paired_system/include',cj]
  sources=[ROOT/'tests/pair_files_host.c',ROOT/'components/paired_system/files.c',ROOT/'components/paired_system/json.c',ROOT/'components/cyd_common/json.c',ROOT/'components/cyd_common/zip.c',cj/'cJSON.c']
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=undefined','-fno-sanitize-recover=all','-DCYD_ROOT="'+str(cls.root)+'"']+['-I'+str(p)for p in inc]+[str(p)for p in sources]+['-lm','-lcrypto','-o',str(out)],check=True)
  cls.lib=C.CDLL(str(out));cls.lib.cyd_pair_files.argtypes=[C.c_uint,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)];cls.lib.files_flags.argtypes=[C.c_int,C.c_int]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def setUp(self):
  import shutil
  for p in self.root.iterdir():
   if p.is_symlink()or p.is_file():p.unlink()
   else:shutil.rmtree(p)
  (self.root/'documents').mkdir();(self.root/'reports').mkdir();(self.root/'projects').mkdir();self.lib.files_flags(1,0)
 def call(self,op,query):
  b=json.dumps(query,ensure_ascii=False).encode()if isinstance(query,dict)else query;out=C.create_string_buffer(32768);size=C.c_size_t();e=self.lib.cyd_pair_files(op,b,len(b),out,len(out),C.byref(size));return e,out.raw[:size.value]
 def test_roots_create_edit_rename_delete(self):
  e,b=self.call(LIST,{'path':''});self.assertEqual(e,0);self.assertEqual([x['name']for x in json.loads(b)],['documents','reports','projects','data','backups'])
  q={'path':'documents/note.txt','offset':0,'text':'hello °'};self.assertEqual(self.call(WRITE,q)[0],0);self.assertNotEqual(self.call(WRITE,q)[0],0)
  self.assertEqual(self.call(READ,{'path':q['path'],'offset':0,'length':2048}),(0,'hello °'.encode()))
  q.update(text='edited',replace=True);self.assertEqual(self.call(WRITE,q)[0],0);self.assertEqual((self.root/q['path']).read_bytes(),b'edited')
  self.assertEqual(self.call(RENAME,{'path':q['path'],'target':'documents/renamed.txt'})[0],0);self.assertEqual(self.call(DELETE,{'path':'documents/renamed.txt'})[0],0)
 def test_paths_and_readonly_system_areas(self):
  for p in ['../secret','/etc/passwd','documents/../data/file','Documents/a','documents/.hidden','documents/a.previous','system/catalog','data/private','projects/app']:
   self.assertNotEqual(self.call(WRITE,{'path':p,'offset':0,'text':'bad'})[0],0,p)
  (self.root/'projects'/'readme.txt').write_bytes(b'kept');self.assertEqual(self.call(READ,{'path':'projects/readme.txt','offset':0,'length':20}),(0,b'kept'))
  self.assertNotEqual(self.call(READ,b'{"path":"documents/a\\u0000b","offset":0,"length":1}')[0],0)
 def test_symlink_files_parents_and_stages(self):
  private=Path(self.tmp.name)/'private';private.write_bytes(b'PRIVATE')
  (self.root/'documents'/'link.txt').symlink_to(private)
  (self.root/'documents'/'linked').symlink_to(private.parent,target_is_directory=True)
  for p in ['documents/link.txt','documents/linked/private']:
   self.assertNotEqual(self.call(READ,{'path':p,'offset':0,'length':20})[0],0)
  (self.root/'documents'/'dest.txt.incoming').symlink_to(private)
  packet=struct.pack('<IH',0,len('documents/dest.txt'))+b'documents/dest.txt'+b'evil'
  self.assertNotEqual(self.call(PUT,packet)[0],0);self.assertEqual(private.read_bytes(),b'PRIVATE')
  self.assertNotEqual(self.call(COMMIT,{'path':'documents/dest.txt','size':7,'sha256':hashlib.sha256(b'PRIVATE').hexdigest()})[0],0)
 def test_chunked_copy_exact_offsets_hash_and_commit(self):
  data=bytes(range(256))*17;path=b'documents/copy.bin';offset=0
  for start in range(0,len(data),2048):
   chunk=data[start:start+2048];packet=struct.pack('<IH',offset,len(path))+path+chunk
   self.assertEqual(self.call(PUT,packet)[0],0);offset+=len(chunk)
  self.assertFalse((self.root/path.decode()).exists());self.assertEqual(json.loads(self.call(LIST,{'path':'documents'})[1]),[])
  self.assertNotEqual(self.call(PUT,struct.pack('<IH',1,len(path))+path+b'x')[0],0)
  q={'path':path.decode(),'size':len(data),'sha256':'0'*64};self.assertNotEqual(self.call(COMMIT,q)[0],0)
  q['sha256']=hashlib.sha256(data).hexdigest();self.assertEqual(self.call(COMMIT,q)[0],0);self.assertEqual((self.root/path.decode()).read_bytes(),data)
  self.assertNotEqual(self.call(PUT,struct.pack('<IH',0,len(path))+path+b'x')[0],0)
 def test_edit_journal_recovery(self):
  (self.root/'documents'/'note.txt.previous').write_bytes(b'saved');(self.root/'documents'/'note.txt.editing').write_bytes(b'partial')
  self.assertEqual(self.call(READ,{'path':'documents/note.txt','offset':0,'length':10}),(0,b'saved'));self.assertFalse((self.root/'documents'/'note.txt.editing').exists())
 def test_maintenance_missing_card_bounds_and_no_clobber(self):
  self.lib.files_flags(1,1);self.assertNotEqual(self.call(WRITE,{'path':'documents/a','offset':0,'text':'x'})[0],0);self.assertEqual(self.call(LIST,{'path':''})[0],0)
  self.lib.files_flags(0,0);self.assertNotEqual(self.call(LIST,{'path':''})[0],0);self.lib.files_flags(1,0)
  for name in ['a','b']:(self.root/'documents'/name).write_text(name)
  self.assertNotEqual(self.call(RENAME,{'path':'documents/a','target':'documents/b'})[0],0);self.assertEqual((self.root/'documents'/'b').read_text(),'b')
  self.assertNotEqual(self.call(READ,{'path':'documents/a','offset':-1,'length':1})[0],0)
  self.assertNotEqual(self.call(READ,{'path':'documents/a','offset':0,'length':2049})[0],0)
 def test_list_capacity_explicit_error(self):
  for i in range(65):(self.root/'documents'/f'n{i}.txt').write_text('x')
  self.assertNotEqual(self.call(LIST,{'path':'documents'})[0],0)
