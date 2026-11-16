"""Real S3-scoped CMD59 retry preserves integrity failures and other errors."""
from pathlib import Path
import subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[1]
class SDCrc(unittest.TestCase):
    def test_exact_retry_and_no_crc_bypass(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp)/'sd-crc'
            subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-fno-sanitize-recover=all','-I'+str(ROOT/'tests/sd_crc_sdk'),
                str(ROOT/'tests/sd_crc_host.c'),'-o',str(binary)],check=True)
            result = subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('PASS exact CMD59 retry',result.stdout)
