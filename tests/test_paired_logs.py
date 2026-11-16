"""Actual paired C5 runtime uses external memory for the 64-line history."""
from pathlib import Path
import os, subprocess, unittest
ROOT = Path(__file__).resolve().parents[1]

class PairedLogs(unittest.TestCase):
    def test_log_memory_failure_wrap_and_runtime_lifecycle(self):
        if not os.getenv('IDF_PATH'):
            self.skipTest('ESP-IDF cJSON required')
        build = ROOT / 'build-platform-host'
        subprocess.run(['cmake', '-S', str(ROOT / 'tests/platform_host'), '-B', str(build)], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(['cmake', '--build', str(build), '--target', 'paired_logs_host', '--parallel', '4'], check=True, stdout=subprocess.DEVNULL)
        result = subprocess.run([str(build / 'paired_logs_host')], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PASS C5 PSRAM log history', result.stdout)
