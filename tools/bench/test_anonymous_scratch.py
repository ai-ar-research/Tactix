"""Crash/timeout/kill controls use synthetic writers, never SAT benchmarks."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
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
