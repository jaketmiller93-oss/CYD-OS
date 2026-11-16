"""Real HTTP startup sequencer against the observed 9 KiB internal-heap limit."""
from pathlib import Path
import subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[1]
class WebStartup(unittest.TestCase):
    def test_early_task_reservation_and_late_route_registration(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / 'web-startup'
            subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                '-fno-sanitize-recover=all', '-I'+str(ROOT/'tests/web_startup_sdk'),
                str(ROOT/'tests/web_startup_host.c'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('PASS exact HTTPD task failure', result.stdout)
