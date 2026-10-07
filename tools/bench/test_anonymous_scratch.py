"""Crash/timeout/kill controls use synthetic writers, never SAT benchmarks."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import anonymous_scratch
from anonymous_scratch import AnonymousScratch

class ScratchTests(unittest.TestCase):
    def test_missing_python_constant_keeps_anonymous_storage(self):
        original = getattr(os, 'O_TMPFILE', 0o20000000 | os.O_DIRECTORY)
        with mock.patch.dict(os.__dict__):
            os.__dict__.pop('O_TMPFILE', None)
            with mock.patch.object(anonymous_scratch.sys, 'platform', 'linux'), mock.patch.object(anonymous_scratch.platform, 'machine', return_value='x86_64'):
                self.assertEqual(anonymous_scratch.anonymous_open_flag(), original)
                self.test_reopen_and_close()

    def test_missing_constant_unknown_abi_fails_closed(self):
        with mock.patch.dict(os.__dict__):
            os.__dict__.pop('O_TMPFILE', None)
            with mock.patch.object(anonymous_scratch.platform, 'machine', return_value='unknown'):
                with self.assertRaises(OSError):
                    AnonymousScratch('/tmp') / 'proof'

    def test_reopen_and_close(self):
        with tempfile.TemporaryDirectory() as tmp:
            s=AnonymousScratch(tmp)
            try:
                p=s/'input.cnf'; p.write_bytes(b'formula')
                self.assertEqual(p.stat().st_nlink,0)
                self.assertEqual(subprocess.check_output(['cat',str(p)]),b'formula')
                self.assertEqual(list(Path(tmp).iterdir()),[])
            finally:s.close()
            self.assertFalse(p.exists())
            s.close()

    def test_release_returns_blocks_and_preserves_other_handles(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = AnonymousScratch(tmp)
            first, second = s/'first', s/'second'
            first.write_bytes(b'a' * (1 << 20))
            second.write_bytes(b'keep')
            self.assertTrue(s.release('first'))
            self.assertFalse(first.exists())
            self.assertEqual(second.read_bytes(), b'keep')
            self.assertFalse(s.release('first'))
            with self.assertRaises(TypeError):
                s.release(first)
            with self.assertRaisesRegex(RuntimeError, 'cannot be reopened'):
                s/'first'
            replacement = s/'replacement'
            replacement.write_bytes(b'new')
            # Linux normally reuses the just-closed descriptor. A stale proc
            # path may therefore name the replacement, but it is never an
            # accepted release identity and duplicate logical cleanup is safe.
            self.assertEqual(replacement.read_bytes(), b'new')
            self.assertFalse(s.release('first'))
            self.assertEqual(replacement.read_bytes(), b'new')
            s.close()

    def test_release_closes_and_tombstones_after_truncate_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = AnonymousScratch(tmp)
            artifact = s/'artifact'
            artifact.write_bytes(b'content')
            with mock.patch.object(os, 'ftruncate', side_effect=OSError('injected truncate failure')):
                with self.assertRaisesRegex(OSError, 'injected truncate failure'):
                    s.release('artifact')
            self.assertFalse(artifact.exists())
            self.assertFalse(s.contains('artifact'))
            with self.assertRaisesRegex(RuntimeError, 'cannot be reopened'):
                s/'artifact'
            s.close()

    def test_capacity_admission_fails_before_opening_an_artifact(self):
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(os, 'statvfs', return_value=SimpleNamespace(
                 f_bavail=399, f_frsize=1)), \
             mock.patch.object(os, 'open') as opened:
            with self.assertRaisesRegex(RuntimeError, 'scratch_admission') as caught:
                AnonymousScratch(
                    tmp, minimum_free_bytes=400, maximum_file_bytes=60,
                    maximum_live_handles=6)
            self.assertEqual(caught.exception.scratch_reason,
                             'scratch_admission')
            opened.assert_not_called()

    def test_capacity_admission_accepts_exact_available_boundary(self):
        import resource
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(os, 'statvfs', return_value=SimpleNamespace(
                 f_bavail=400, f_frsize=1)), \
             mock.patch.object(resource, 'getrlimit', side_effect=[
                 (resource.RLIM_INFINITY, resource.RLIM_INFINITY),
                 (60, 60), (60, 60)]), \
             mock.patch.object(resource, 'setrlimit'):
            scratch = AnonymousScratch(
                tmp, minimum_free_bytes=400, maximum_file_bytes=60,
                maximum_live_handles=6)
            self.assertEqual(scratch.resource_status()['available_bytes'], 400)
            scratch.close()

    def test_handle_cap_is_atomic_and_release_preserves_high_water(self):
        import resource
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(resource, 'getrlimit', side_effect=[
                 (resource.RLIM_INFINITY, resource.RLIM_INFINITY),
                 (1024, 1024), (1024, 1024)]), \
             mock.patch.object(resource, 'setrlimit'):
            s = AnonymousScratch(
                tmp, minimum_free_bytes=1, maximum_file_bytes=1024,
                maximum_live_handles=2)
            first, second = s/'first', s/'second'
            with self.assertRaisesRegex(RuntimeError, 'scratch_handle_limit'):
                s/'third'
            self.assertEqual(set(s.handles), {'first', 'second'})
            self.assertTrue(s.release('first'))
            third = s/'third'
            self.assertTrue(second.exists() and third.exists())
            status = s.resource_status()
            self.assertEqual(status['high_water_handles'], 2)
            self.assertEqual(status['active_handles'], 2)
            with self.assertRaisesRegex(RuntimeError, 'cannot be reopened'):
                s/'first'
            s.close()

    def test_repeated_stage_names_use_distinct_tombstoned_generations(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = AnonymousScratch(tmp)
            first_name = s.fresh_name('stage.stdout')
            first = s/first_name
            first.write_bytes(b'old')
            self.assertTrue(s.release(first_name))
            second_name = s.fresh_name('stage.stdout')
            second = s/second_name
            second.write_bytes(b'new')
            self.assertNotEqual(first_name, second_name)
            # The kernel may reuse the descriptor, so the stale proc path can
            # alias this generation. Only its distinct logical name is valid.
            self.assertFalse(s.release(first_name))
            self.assertEqual(second.read_bytes(), b'new')
            s.close()

    def test_file_limit_is_exact_and_inherited_by_children(self):
        script = r'''import os, resource, signal, subprocess, sys, tempfile
from anonymous_scratch import AnonymousScratch
with tempfile.TemporaryDirectory() as tmp:
 s=AnonymousScratch(tmp,minimum_free_bytes=1,maximum_file_bytes=4096,maximum_live_handles=2)
 assert resource.getrlimit(resource.RLIMIT_FSIZE)==(4096,4096)
 p=s/'parent-bounded';p.write_bytes(b'x'*4096);assert p.stat().st_size==4096;s.release('parent-bounded')
 p=s/'bounded'
 writer='import os,resource,sys;assert resource.getrlimit(resource.RLIMIT_FSIZE)==(4096,4096);n=int(sys.argv[2]);fd=os.open(sys.argv[1],os.O_WRONLY);os.write(fd,b"x"*min(n,4096));n>4096 and os.write(fd,b"x"*(n-4096));os.close(fd)'
 assert subprocess.run([sys.executable,'-c',writer,str(p),'4096']).returncode==0
 assert p.stat().st_size==4096
 s.release('bounded')
 q=s/'over'
 failed=subprocess.run([sys.executable,'-c',writer,str(q),'4097'])
 assert failed.returncode!=0,failed.returncode
 s.close()
'''
        environment = dict(os.environ, PYTHONPATH=str(Path(__file__).parent))
        completed = subprocess.run(
            [sys.executable, '-c', script], env=environment,
            capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_abnormal_exit(self):
        for mode in ['crash','timeout','term','kill']:
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                script='''import sys,os,time
from anonymous_scratch import AnonymousScratch
s=AnonymousScratch(sys.argv[1])
for name in ['input.cnf','proof.drat','solver.stdout']:
 p=s/name;p.write_bytes(b'x'*1048576)
 print(str(p),flush=True)
if sys.argv[2]=='crash':os._exit(7)
time.sleep(60)
'''
                env=dict(os.environ, PYTHONPATH=str(Path(__file__).resolve().parent))
                p=subprocess.Popen([sys.executable,'-c',script,tmp,mode],stdout=subprocess.PIPE,text=True,env=env)
                paths=[Path(p.stdout.readline().strip()) for _ in range(3)]
                try:
                    self.assertEqual(list(Path(tmp).iterdir()),[])
                    if mode=='timeout':
                        with self.assertRaises(subprocess.TimeoutExpired):p.wait(timeout=.05)
                        p.kill()
                    elif mode=='kill':p.kill()
                    elif mode=='term':p.terminate()
                    p.wait(timeout=5)
                    self.assertTrue(all(not path.exists() for path in paths))
                    self.assertEqual(list(Path(tmp).iterdir()),[])
                finally:
                    if p.poll() is None:p.kill();p.wait()
                    p.stdout.close()

    def test_unsupported_fails_closed(self):
        with self.assertRaises(OSError):
            AnonymousScratch('/proc')/'proof'

if __name__=='__main__':unittest.main()
