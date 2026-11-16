"""Use Espressif's actual 32-bit compiler: LP64 host builds hide long/long-long mismatches."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class LuaAbi(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  compiler=os.getenv('CYD_C5_CC') or shutil.which('riscv32-esp-elf-gcc')
  if not compiler:
   installed=sorted((Path.home()/'.espressif/tools/riscv32-esp-elf').glob('*/riscv32-esp-elf/bin/riscv32-esp-elf-gcc'))
   if installed:compiler=str(installed[-1])
  if not compiler:raise unittest.SkipTest('Espressif 32-bit compiler unavailable')
  cls.compiler=compiler;cls.tmp=tempfile.TemporaryDirectory();cls.base=Path(cls.tmp.name)
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def compile(self,source,flags=()):
  p=self.base/'probe.c';p.write_text(source)
  return subprocess.run([self.compiler,'-std=c17','-march=rv32imac','-mabi=ilp32','-Werror','-I'+str(ROOT/'components/lua/vendor'),*flags,'-c',str(p),'-o',str(self.base/'probe.o')],capture_output=True,text=True)
 def test_real_c5_default_types_match_binding_contract(self):
  r=self.compile('#include "lua.h"\n_Static_assert(sizeof(long)==4,"requires real 32-bit target");\n_Static_assert(sizeof(lua_Integer)==8,"Lua integer ABI");\n_Static_assert(sizeof(lua_Number)==8,"Lua float ABI");\n')
  self.assertEqual(r.returncode,0,r.stderr)
 def test_old_private_c89_mode_reproduces_wrong_integer_width(self):
  r=self.compile('#include "lua.h"\n_Static_assert(sizeof(long)==4,"32-bit target");\n_Static_assert(sizeof(lua_Integer)==4,"old C89 mode must expose bug");\n',['-DLUA_USE_C89'])
  self.assertEqual(r.returncode,0,r.stderr)
 def test_library_guard_rejects_regression_on_real_c5_compiler(self):
  guard=(ROOT/'components/lua/abi.c').read_text()
  r=self.compile(guard,['-DLUA_USE_C89']);self.assertNotEqual(r.returncode,0);self.assertIn('64-bit integers',r.stderr)
 def test_library_guard_accepts_consistent_default_configuration(self):
  r=self.compile((ROOT/'components/lua/abi.c').read_text());self.assertEqual(r.returncode,0,r.stderr)
