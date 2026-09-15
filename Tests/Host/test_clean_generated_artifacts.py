"""Disposable-repository tests for ignored-only cleanup; no physical I/O."""
import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest

SOURCE = (Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 and sys.argv[1].endswith('.py')
          else Path(__file__).resolve().parents[2]/'tools/clean_generated_artifacts.py')
spec = importlib.util.spec_from_file_location('clean_artifacts', SOURCE)
clean = importlib.util.module_from_spec(spec); spec.loader.exec_module(clean)


class CleanupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sd700-ignored-clean-test-')
        self.root = Path(self.temp.name).resolve()
        self.git('init', '-q')
        self.put('.gitignore', b'/output/\n')
        self.put('control.c', b'unchanged controller')
        self.put('tools/test_input.py', b"gold = ROOT/'output/fixtures/gold.json'\n")
        self.put('output/current/firmware/current.hex', b'tracked current firmware')
        self.put('output/history/firmware/old.elf', b'tracked historical firmware')
        self.git('add', '-f', '.')
        self.git('-c', 'user.name=CleanupTest', '-c', 'user.email=test@example.invalid',
                 '-c', 'commit.gpgsign=false', 'commit', '-qm', 'fixture')
        self.put('output/fixtures/gold.json', b'{"fixture":true}')
        for p in ('output/arm/obj/a.o', 'output/arm/a.d', 'output/arm/a.map',
                  'output/host/test.exe', 'output/repro-before/fail.log',
                  'output/verification-final/test_execution.json', 'output/dev-1/__pycache__/module.pyc'):
            self.put(p, b'regenerable')
        meta = dict(firmware_sha256='SYNTHETIC_NO_HARDWARE', initial_gap='SYNTHETIC',
                    current_limit_setting='SYNTHETIC', physical_test_status='OPERATOR_CAPTURE_UNVALIDATED')
        self.family('output/verification-final/capture/SYNTHETIC', meta)

    def tearDown(self):
        self.temp.cleanup()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.STDOUT)

    def put(self, name, data):
        p = self.root/name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(data)

    def family(self, stem, metadata):
        self.put(stem+'.csv', b'force\n25\n')
        self.put(stem+'.metadata.json', json.dumps(metadata).encode())
        self.put(stem+'.report.txt', b'report')

    def apply(self, plan, idle=lambda root: []):
        return clean.apply_plan(self.root, plan, idle_check=idle)

    def test_dry_run_then_exact_apply_and_clean_status(self):
        plan = clean.snapshot(self.root)
        self.assertEqual(plan['removed_files'], 10)
        self.assertEqual(plan['expected_after_files'], 3)
        self.assertEqual(plan['git_status'], '')
        self.assertTrue(all((self.root/e['path']).is_file() for e in plan['entries']))
        result = self.apply(plan)
        self.assertEqual(result['after_size'], plan['expected_after_bytes'])
        self.assertEqual(self.git('status', '--porcelain'), b'')
        self.assertTrue((self.root/'output/fixtures/gold.json').exists())
        self.assertFalse((self.root/'output/verification-final').exists())

    def test_real_and_uncertain_captures_survive_generated_directories(self):
        self.family('output/dev-physical/real', dict(firmware_sha256='A'*64))
        self.family('output/dev-misnamed/SYNTHETIC', dict(firmware_sha256='B'*64,
                    initial_gap='SYNTHETIC', current_limit_setting='SYNTHETIC'))
        self.put('output/host/unknown.csv', b'force\n0\n')
        self.put('output/captures/opaque.bin', b'field attachment')
        plan = clean.snapshot(self.root)
        self.assertEqual(sum(r['classification'] != 'SYNTHETIC' for r in plan['capture_audit']), 3)
        self.apply(plan)
        for p in ('output/dev-physical/real.csv', 'output/dev-misnamed/SYNTHETIC.metadata.json',
                  'output/host/unknown.csv', 'output/captures/opaque.bin'):
            self.assertTrue((self.root/p).exists(), p)

    def test_nonignored_and_explicit_keep_survive(self):
        self.put('.gitignore', b'/output/\n!/output/\n/output/*\n!/output/notes.txt\n')
        self.put('output/notes.txt', b'not ignored')
        self.put('output/extra/computed.bin', b'computed fixture')
        (self.root/'output/empty-fixture').mkdir()
        plan = clean.snapshot(self.root, ['output/extra', 'output/empty-fixture'])
        self.apply(plan)
        self.assertTrue((self.root/'output/notes.txt').exists())
        self.assertTrue((self.root/'output/extra/computed.bin').exists())
        self.assertTrue((self.root/'output/empty-fixture').is_dir())

    def test_changed_hash_ignore_policy_or_plan_refused_before_deletion(self):
        plan = clean.snapshot(self.root)
        for kind in ('file', 'plan', 'ignore'):
            with self.subTest(kind=kind):
                original = (self.root/'output/arm/a.map').read_bytes()
                if kind == 'file': self.put('output/arm/a.map', b'changed')
                elif kind == 'plan': plan['entries'][0]['sha256'] = '0'*64
                else: self.put('.gitignore', b'/output/arm/\n')
                with self.assertRaisesRegex(ValueError, 'Inventory changed'):
                    self.apply(plan)
                self.assertTrue((self.root/'output/host/test.exe').exists())
                self.put('output/arm/a.map', original); self.put('.gitignore', b'/output/\n')
                plan = clean.snapshot(self.root)

    def test_new_physical_capture_after_plan_blocks_all_deletion(self):
        plan = clean.snapshot(self.root)
        self.family('output/verification-final/live', dict(firmware_sha256='C'*64))
        with self.assertRaisesRegex(ValueError, 'Inventory changed'):
            self.apply(plan)
        self.assertTrue((self.root/'output/arm/a.map').exists())

    def test_active_task_refused(self):
        plan = clean.snapshot(self.root)
        with self.assertRaisesRegex(ValueError, 'Active build/test/capture'):
            self.apply(plan, lambda root: [dict(pid=1, name='gcc.exe')])
        self.assertTrue((self.root/'output/arm/a.map').exists())

    def test_only_disposable_nested_clone_git_removed(self):
        self.put('output/old/clone_verification/.git/objects/pack/data.pack', b'disposable pack')
        pack = self.root/'output/old/clone_verification/.git/objects/pack/data.pack'
        os.chmod(pack, stat.S_IREAD)
        self.put('output/user_repo/.git/config', b'unknown repo keep')
        top_config = (self.root/'.git/config').read_bytes()
        plan = clean.snapshot(self.root); self.apply(plan)
        self.assertFalse((self.root/'output/old/clone_verification').exists())
        self.assertEqual((self.root/'.git/config').read_bytes(), top_config)
        self.assertTrue((self.root/'output/user_repo/.git/config').exists())

    def test_unsafe_paths_and_junction(self):
        for p in ('../control.c', 'Reference/thing', '.git/config', 'output/../control.c',
                  'output/x/.git/config', 'output//x', 'output./x', 'output/x:stream', 'output/x. /a'):
            with self.subTest(path=p), self.assertRaises(ValueError):
                clean.safe_output(self.root, p)
        outside = self.root/'outside'; outside.mkdir(); (outside/'keep.bin').write_bytes(b'keep')
        link = self.root/'output/junction'
        quote = lambda p: "'"+str(p).replace("'", "''")+"'"
        command = 'New-Item -ItemType Junction -Path '+quote(link)+' -Target '+quote(outside)+' | Out-Null'
        subprocess.run(['powershell.exe', '-NoProfile', '-Command', '-'], input=command, text=True, capture_output=True, check=True)
        try:
            self.assertTrue(link.is_junction())
            with self.assertRaisesRegex(ValueError, 'junction'):
                clean.safe_output(self.root, 'output/junction/keep.bin')
            self.apply(clean.snapshot(self.root))
            self.assertEqual((outside/'keep.bin').read_bytes(), b'keep')
        finally:
            self.assertTrue(link.resolve().is_relative_to(self.root)); link.rmdir()


if __name__ == '__main__':
    unittest.main(verbosity=2)
