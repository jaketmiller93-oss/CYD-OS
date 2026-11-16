"""Actual Lua sandbox budgets distinguish slow native UI from runaway scripts."""
from pathlib import Path
import os, subprocess, unittest
ROOT = Path(__file__).resolve().parents[1]
class ExecutionBudget(unittest.TestCase):
    def test_native_ui_latency_script_time_wall_cap_and_cancellation(self):
        if not os.getenv('IDF_PATH'): self.skipTest('ESP-IDF cJSON required')
        build = ROOT/'build-platform-host'
        subprocess.run(['cmake','-S',str(ROOT/'tests/platform_host'),'-B',str(build)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['cmake','--build',str(build),'--target','budget_host','--parallel','4'],check=True,stdout=subprocess.DEVNULL)
        result = subprocess.run([str(build/'budget_host')],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('PASS slow UI',result.stdout)
