"""Cleanup rejection tests in disposable fake repositories, never real evidence."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
import cleanup_generated as cleanup


class CleanupTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sd700-cleanup-test-')
        self.root = Path(self.tmp.name).resolve()
        self.run_git('init', '-q')
        self.put('tracked.c', b'source')
        self.put('output/host/tracked.exe', b'fixture')
        self.put(cleanup.PDF, b'datasheet')
        self.put(cleanup.PDF_REFERENCE, b'datasheet')
        self.run_git('add', '.')
        self.run_git('-c', 'user.name=CleanupTest', '-c', 'user.email=fixture@example.invalid',
                     '-c', 'commit.gpgsign=false', 'commit', '-qm', 'fixture')
        for p in ('output/dev/obj/main.o', 'output/dev/obj/main.d', 'output/host/generated.exe',
                  'tools/__pycache__/module.pyc'):
            self.put(p, b'generated')
        for p in ('output/dev/capture.csv', 'output/dev/capture.metadata.json',
                  'output/dev/capture.report.txt', 'output/dev/check.log',
                  'output/firmware/unique.elf', 'output/firmware/unique.hex',
                  'output/verification-final/host/test.exe', 'output/repro-before/obj/main.o',
                  'Reference/LegacySource/legacy.o'):
            self.put(p, b'evidence')

    def tearDown(self):
        self.tmp.cleanup()

    def run_git(self, *args):
        subprocess.run(['git', '-C', str(self.root), *args], check=True, capture_output=True)

    def put(self, p, data):
        target = self.root/p; target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(data)

    def plan(self):
        return cleanup.make_plan(self.root)

    def apply(self, plan, idle=lambda root: []):
        return cleanup.apply_plan(self.root, plan, idle_check=idle)

    def test_dry_run_and_exact_apply_preserve_evidence(self):
        plan = self.plan()
        self.assertEqual(plan['mode'], 'DRY_RUN')
        self.assertEqual(plan['delete_files'], 5)
        for e in plan['entries']:
            self.assertTrue((self.root/e['path']).is_file())
        result = self.apply(plan)
        self.assertEqual(result['retained_before'], result['retained_after'])
        for e in plan['entries']:
            self.assertFalse((self.root/e['path']).exists())
        self.assertTrue((self.root/cleanup.PDF_REFERENCE).is_file())
        self.assertTrue((self.root/'output/host/tracked.exe').is_file())
        self.assertTrue((self.root/'output/dev/capture.csv').is_file())

    def test_changed_candidate_preflight_deletes_nothing(self):
        plan = self.plan(); self.put(plan['entries'][-1]['path'], b'changed')
        with self.assertRaisesRegex(ValueError, 'Cleanup file changed'):
            self.apply(plan)
        self.assertTrue(all((self.root/e['path']).exists() for e in plan['entries']))

    def test_changed_retained_bytes_preflight_deletes_nothing(self):
        plan = self.plan(); self.put('output/dev/check.log', b'new evidence')
        with self.assertRaisesRegex(ValueError, 'Retained bytes changed'):
            self.apply(plan)
        self.assertTrue((self.root/cleanup.PDF).exists())

    def test_protected_paths_cannot_be_added_to_plan(self):
        for p in ('tracked.c', 'output/host/tracked.exe', 'output/firmware/unique.elf',
                  'output/dev/capture.csv', 'Reference/LegacySource/legacy.o'):
            with self.subTest(path=p):
                plan = self.plan(); file = self.root/p
                entry = dict(path=p, bytes=file.stat().st_size, sha256=cleanup.sha(file),
                             reason='regenerable_object_or_dependency')
                plan['entries'].append(entry); plan['delete_files'] += 1; plan['delete_bytes'] += entry['bytes']
                with self.assertRaisesRegex(ValueError, 'Protected/ineligible'):
                    self.apply(plan)
                self.assertTrue((self.root/cleanup.PDF).is_file())

    def test_traversal_absolute_git_and_aliases_rejected(self):
        for p in ('../outside.o', '/absolute.o', 'C:/outside.o', 'output/../tracked.c',
                  '.git/config', '.GIT/config', 'output//a.o', 'output/./a.o', 'output\\a.o'):
            with self.subTest(path=p), self.assertRaises(ValueError):
                cleanup.safe_path(self.root, p)

    def test_windows_junction_rejected(self):
        target = self.root/'Reference'; link = self.root/'output/link'
        script = "New-Item -ItemType Junction -Path '"+str(link).replace("'", "''")+"' -Target '"+str(target).replace("'", "''")+"' | Out-Null"
        subprocess.run(['powershell.exe', '-NoProfile', '-Command', '-'], input=script,
                       capture_output=True, text=True, check=True)
        self.assertTrue(link.is_junction())
        try:
            with self.assertRaisesRegex(ValueError, 'Link/junction'):
                cleanup.safe_path(self.root, 'output/link/LegacySource/legacy.o')
            self.assertNotIn('output/link/LegacySource/legacy.o', list(cleanup.walk_files(self.root, 'output')))
        finally:
            self.assertTrue(link.resolve().is_relative_to(self.root))
            link.rmdir()  # Remove the junction itself, never its target tree.

    def test_active_task_blocks_before_first_delete(self):
        plan = self.plan()
        with self.assertRaisesRegex(ValueError, 'Active build/test/capture'):
            self.apply(plan, lambda root: [dict(pid=123, name='arm-none-eabi-gcc.exe')])
        self.assertTrue(all((self.root/e['path']).is_file() for e in plan['entries']))

    def test_mismatched_pdf_refused(self):
        self.put(cleanup.PDF_REFERENCE, b'different source')
        with self.assertRaisesRegex(ValueError, 'not an exact duplicate'):
            self.plan()

    def test_failed_and_current_final_runs_preserved(self):
        self.put('output/iteration/obj/main.o', b'failed binary')
        self.put('output/iteration/test_execution.json', json.dumps([dict(exit_code=1, result='FAIL')]).encode())
        self.put('output/release/obj/main.o', b'final binary')
        self.put('Docs/BuildToTarget2_CoolingAnchorFix1/verification.json',
                 json.dumps([dict(log='output/release/check.log')]).encode())
        plan = self.plan(); removed = {e['path'] for e in plan['entries']}
        self.assertNotIn('output/iteration/obj/main.o', removed)
        self.assertNotIn('output/release/obj/main.o', removed)

    def test_head_or_totals_change_rejected(self):
        plan = self.plan()
        for key, value in (('head', 'wrong'), ('delete_files', 999)):
            changed = copy.deepcopy(plan); changed[key] = value
            with self.assertRaises(ValueError):
                self.apply(changed)
        self.assertTrue((self.root/cleanup.PDF).is_file())


if __name__ == '__main__':
    unittest.main(verbosity=2)
