"""Independent reference encoder vs actual C header parser, including corruption."""
import ctypes as C
from pathlib import Path
import random,struct,subprocess,tempfile,unittest,zlib
ROOT=Path(__file__).resolve().parents[1]
class Image(C.Structure):_fields_=[('board',C.c_char*24),('version',C.c_char*32),('size',C.c_uint32),('offset',C.c_uint32),('sha256',C.c_ubyte*32)]
class Bundle(C.Structure):_fields_=[('version',C.c_char*32),('images',Image*2)]
class PairBundle(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();out=Path(cls.tmp.name)/'bundle.so'
  subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'components/paired_system/include'),str(ROOT/'components/paired_system/bundle.c'),'-o',str(out)],check=True)
  cls.lib=C.CDLL(str(out));cls.lib.cyd_bundle_parse.argtypes=[C.c_void_p,C.c_size_t,C.c_size_t,C.POINTER(Bundle)];cls.lib.cyd_bundle_parse.restype=C.c_bool
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def header(self):
  b=bytearray(512);struct.pack_into('<16sIIII32s',b,0,b'CYD-PAIR-UPDATE\0',1,2,3584,0,b'0.5.0')
  struct.pack_into('<24s32sII32s',b,64,b'crowpanel_s3',b'0.5.0',1024,512,bytes(range(32)))
  struct.pack_into('<24s32sII32s',b,192,b'nm_cyd_c5',b'0.5.0',2048,1536,bytes(reversed(range(32))))
  return self.crc(b)
 def crc(self,b):struct.pack_into('<I',b,508,zlib.crc32(b[:508]));return b
 def parse(self,b,total=3584):
  out=Bundle();ok=self.lib.cyd_bundle_parse(bytes(b),len(b),total,C.byref(out));return ok,out
 def test_reference_header_and_roles(self):
  ok,b=self.parse(self.header());self.assertTrue(ok);self.assertEqual(b.version,b'0.5.0');self.assertEqual(b.images[1].board,b'nm_cyd_c5');self.assertEqual(b.images[0].offset,512)
 def test_every_single_bit_corruption_rejected(self):
  original=self.header()
  for i in range(512):
   b=bytearray(original);b[i]^=1;self.assertFalse(self.parse(b)[0],i)
 def test_version_and_board_skew_rejected_even_with_valid_crc(self):
  for offset,text in [(64,b'nm_cyd_c5\0'),(192,b'crowpanel_s3\0'),(192+24,b'0.5.1\0')]:
   b=self.header();b[offset:offset+len(text)]=text;self.assertFalse(self.parse(self.crc(b))[0])
 def test_offsets_sizes_and_exact_total(self):
  for offset,value in [(64+56,0),(64+56,0x700001),(64+60,513),(192+60,0xffffffff),(24,3585),(20,1)]:
   b=self.header();struct.pack_into('<I',b,offset,value);self.assertFalse(self.parse(self.crc(b))[0])
  self.assertFalse(self.parse(self.header(),3583)[0]);self.assertFalse(self.parse(self.header()[:511])[0])
 def test_reserved_bytes_and_noncanonical_padding(self):
  for i in [28,64+96,192+100,320,507,32+10,64+20]:
   b=self.header();b[i]=1;self.assertFalse(self.parse(self.crc(b))[0],i)
 def test_bounded_arbitrary_input(self):
  rng=random.Random(5050)
  for _ in range(2000):
   b=rng.randbytes(rng.randrange(0,600));self.assertFalse(self.parse(b,rng.randrange(0,20*1024*1024))[0])
