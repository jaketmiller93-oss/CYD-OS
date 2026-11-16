"""Container builder must reject a merely renamed standalone or wrong-chip image."""
from pathlib import Path
import hashlib,importlib.util,struct,unittest
from test_pair_update import image
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('paired_package',ROOT/'scripts/package_paired.py');package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
def fixture(board,chip,role):
 b=image(board,chip,role,chip);b[352:417]=hashlib.sha256((ROOT/'partitions.csv').read_bytes()).hexdigest().encode()+b'\0';return bytes(b[:496])+hashlib.sha256(b[:496]).digest()
def resign(b):return bytes(b[:496])+hashlib.sha256(b[:496]).digest()
class PackagePaired(unittest.TestCase):
 def setUp(self):self.s3=fixture('crowpanel_s3',9,1);self.c5=fixture('nm_cyd_c5',0x17,2)
 def test_one_version_two_exact_images_and_hashes(self):
  b,m=package.package(self.s3,self.c5,'0.5.0');self.assertEqual(b[512:],self.s3+self.c5);self.assertEqual(m['sha256'],hashlib.sha256(b).hexdigest());self.assertFalse(m['installed']);self.assertEqual(struct.unpack_from('<I',b,24)[0],len(b))
 def test_chip_or_board_swap_rejected(self):
  with self.assertRaises(ValueError):package.package(self.c5,self.s3,'0.5.0')
 def test_standalone_marker_or_wrong_role_rejected(self):
  for field,value in [(332,7),(417,0),(418,2)]:
   b=bytearray(self.s3);b[field]=value
   with self.assertRaises(ValueError):package.package(resign(b),self.c5,'0.5.0')
 def test_version_skew_and_old_release_line_rejected(self):
  for version in ['0.5.1','0.4.9','0.5.0/evil']:
   with self.assertRaises(ValueError):package.package(self.s3,self.c5,version)
 def test_corrupt_or_merged_image_rejected(self):
  b=bytearray(self.c5);b[200]^=1
  for data in [bytes(b),b'\xff'*512+self.c5]:
   with self.assertRaises(ValueError):package.package(self.s3,data,'0.5.0')
