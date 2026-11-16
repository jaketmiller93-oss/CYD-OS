"""Real manager initializer preserves NVS errors and cleans failed startup resources."""
from pathlib import Path
import os, subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[1]
class PairManagerStartup(unittest.TestCase):
    def test_exact_settings_errors_and_failed_report_task_cleanup(self):
        if not os.getenv('IDF_PATH'):
            self.skipTest('ESP-IDF cJSON required')
        cj = Path(os.environ['IDF_PATH'])/'components/json/cJSON'
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp)/'manager'
            command = ['cc', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            for include in [ROOT/'tests/pair_manager_sdk', ROOT/'tests/pair_sdk', cj,
                ROOT/'components/paired_system/include', ROOT/'components/ota_manager/include', ROOT/'components/device_link/include', ROOT/'components/device_settings/include']:
                command += ['-I'+str(include)]
            subprocess.run(command+[str(ROOT/'tests/pair_manager_startup_host.c'),
                str(ROOT/'components/cyd_common/json.c'), str(cj/'cJSON.c'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            self.assertIn('PASS manager exact NVS error', result.stdout)
