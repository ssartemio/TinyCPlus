"""Verify the bank detects real failures and preserves machine-readable outcomes."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
import xml.etree.ElementTree as ET

from run_modules import ROOT, execute


class ModuleBenchTests(unittest.TestCase):
    def invoke(self, folder, arguments, expected, optimized=False):
        report = folder / 'report.json'
        env = os.environ.copy()
        if optimized:
            env['PYTHONOPTIMIZE'] = '1'
        result = subprocess.run([sys.executable, str(ROOT/'tests/run_modules.py'),
                                 '--module', 'clones', '--report', str(report), *arguments],
                                cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        data = json.loads(report.read_text())
        xml = ET.parse(report.with_suffix('.xml')).getroot()
        self.assertEqual(int(xml.attrib['tests']), len(data['results']))
        self.assertEqual(data['summary']['stages'], len(data['results']))
        for entry in data['results']:
            self.assertTrue(Path(entry['log']).is_file())
        return data, xml

    def test_success_repeat_and_assertions_enabled(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
            data, xml = self.invoke(Path(folder), ['--no-build', '--backend', 'default', '--repeat', '2'],
                                    0, optimized=True)
        self.assertEqual(data['status'], 'passed')
        self.assertEqual(data['summary']['passed'], 3)  # Version check plus two real clone runs.
        self.assertEqual(len({entry['log'] for entry in data['results']}), 3)
        self.assertIn('std/collections.tc', data['source_sha256'])
        self.assertEqual(len(data['compiler_sha256']), 64)
        self.assertEqual(xml.attrib['failures'], '0')

    def test_compiler_failure_is_not_success(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
            data, xml = self.invoke(Path(folder), ['--no-build', '--backend', 'external',
                                                 '--cc', 'tinycplus-intentionally-missing-compiler'], 1)
        self.assertEqual(data['status'], 'failed')
        self.assertEqual(data['summary']['failed'], 1)
        self.assertEqual(len(xml.findall('testcase/failure')), 1)
        self.assertNotEqual(data['results'][-1]['exit_code'], 0)

    def test_failed_build_blocks_suites_instead_of_using_stale_binary(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
            data, xml = self.invoke(Path(folder), ['--backend', 'default',
                                                 '--cc', 'tinycplus-intentionally-missing-compiler'], 1)
        self.assertEqual(data['status'], 'failed')
        self.assertEqual(data['summary']['passed'], 0)
        self.assertEqual(data['summary']['skipped'], 1)
        self.assertEqual(len(xml.findall('testcase/skipped')), 1)
        self.assertEqual(data['results'][-1]['command'][1].endswith('test_collections.py'), True)

    def test_exit_codes_and_missing_program(self):
        result = execute([sys.executable, '-c', 'import sys; print("captured"); sys.exit(7)'],
                         os.environ.copy(), 10)
        self.assertEqual((result['status'], result['exit_code']), ('failed', 7))
        self.assertEqual(result['stdout'], 'captured\n')
        result = execute(['tinycplus-intentionally-missing-program'], os.environ.copy(), 10)
        self.assertEqual((result['status'], result['exit_code']), ('error', 127))

    def test_timeout_is_not_a_pass(self):
        started = time.monotonic()
        result = execute([sys.executable, '-u', '-c', 'import time; print("started"); time.sleep(30)'],
                         os.environ.copy(), 1)
        self.assertEqual((result['status'], result['exit_code']), ('timeout', 124))
        self.assertEqual(result['stdout'], 'started\n')
        self.assertLess(time.monotonic() - started, 10)


if __name__ == '__main__':
    (ROOT/'build').mkdir(exist_ok=True)
    unittest.main()
