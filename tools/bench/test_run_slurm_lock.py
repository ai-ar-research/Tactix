"""Submission must never run an array whose build-pin lock was refused."""

from pathlib import Path
from types import SimpleNamespace
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import run


class SlurmLockTests(unittest.TestCase):
    def submit(self, outdir):
        args = SimpleNamespace(tier='A', partition='cpu-dense-hwulab-q',
                               throttle=None, dry_run=False, wait=False)
        return run.slurm_submit(args, [('one.cnf', 'SAT', 0)],
                                [('candidate', ['solver'], True, True, True)],
                                {'one.cnf': '/fixed/one.cnf'}, 30, 32,
                                str(outdir), '20260925-000000', 'pin', 'test')

    def test_refused_lock_cancels_held_array_before_aggregate(self):
        with tempfile.TemporaryDirectory() as tmp:
            with patch.object(run, 'dsr_checker_pins', return_value={}), \
                 patch.object(run, 'buildlock_gc'), \
                 patch.object(run, 'build_dirs_of', return_value=['/build/pin']), \
                 patch.object(run, 'buildlock_acquire', return_value=2), \
                 patch.object(run, 'sbatch', return_value='123') as submit, \
                 patch.object(run.subprocess, 'run', return_value=
                              subprocess.CompletedProcess([], 0)) as command:
                self.assertEqual(self.submit(Path(tmp)), 1)
            submit.assert_called_once_with(str(Path(tmp)/'array.sbatch'), '--hold')
            command.assert_called_once_with(
                ['scancel', '123'], capture_output=True, text=True)

    def test_success_locks_before_release_and_aggregate(self):
        events = []
        def submitted(path, *extra):
            events.append(('submit', extra))
            return '123' if '--hold' in extra else '124'
        def acquired(*args):
            events.append(('lock', args[1]))
            return 0
        def command(argv, **kwargs):
            events.append((argv[0], argv[1:]))
            return subprocess.CompletedProcess(argv, 0)
        with tempfile.TemporaryDirectory() as tmp:
            with patch.object(run, 'dsr_checker_pins', return_value={}), \
                 patch.object(run, 'buildlock_gc'), \
                 patch.object(run, 'build_dirs_of', return_value=['/build/pin']), \
                 patch.object(run, 'buildlock_acquire', side_effect=acquired), \
                 patch.object(run, 'sbatch', side_effect=submitted), \
                 patch.object(run.subprocess, 'run', side_effect=command):
                self.assertEqual(self.submit(Path(tmp)), 0)
        self.assertEqual([event[0] for event in events],
                         ['submit', 'lock', 'submit', 'scontrol'])
        self.assertEqual(events[-1][1], ['release', '123'])


if __name__ == '__main__':
    unittest.main()
