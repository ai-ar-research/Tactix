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
