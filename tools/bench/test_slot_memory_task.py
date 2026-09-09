"""Fixed fake-runner controls; no solver or scheduler is executed."""
import hashlib
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from slot_memory_task import install_observer, VARIABLE


class SlotMemoryTaskTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.c = Path(self.temp.name)
        (self.c / 'logs').mkdir()
        self.binary = self.c / 'pins/tactix'

    def protocol(self, measured):
        return SimpleNamespace(measured=measured,
            sha256=lambda p: hashlib.sha256(p.read_bytes()).hexdigest())

    def invoke(self, p):
        return p.measured(['taskset', '-c', '0', self.binary], 'out', 'err', 'time', 1200)

    def test_unique_task_outputs_and_metrics_preserved(self):
        paths = []
        def measured(*args):
            paths.append(Path(os.environ[VARIABLE]))
            paths[-1].write_text('Eliminate\tleave\tmax_rss_kib=1\n')
            return dict(exit_code=10, wall_seconds=3, timed_out=False)
        with patch.dict(os.environ, {}, clear=True):
            for task in (0, 1):
                p = self.protocol(measured)
                install_observer(p, self.c, task, self.binary)
                r = self.invoke(p)
                self.assertEqual(r['exit_code'], 10)
                self.assertEqual(r['wall_seconds'], 3)
                self.assertEqual(r['memory_trace']['sha256'], p.sha256(paths[-1]))
                self.assertGreater(r['memory_trace']['bytes'], 0)
                self.assertNotIn(VARIABLE, os.environ)
        self.assertNotEqual(*paths)

    def test_checker_is_untouched(self):
        calls = []
        def measured(*args):
            self.assertNotIn(VARIABLE, os.environ)
            calls.append(args)
            return dict(exit_code=0)
        p = self.protocol(measured)
        install_observer(p, self.c, 0, self.binary)
        args = ([self.c / 'pins/checkmodel'], 'out', 'err', 'time', 16000)
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(p.measured(*args), dict(exit_code=0))
        self.assertEqual(calls, [args])

    def test_environment_restored_on_failure(self):
        def measured(*args):
            raise RuntimeError('fixture')
        for previous in (None, 'caller-value'):
            with patch.dict(os.environ, {} if previous is None else {VARIABLE: previous}, clear=True):
                p = self.protocol(measured)
                install_observer(p, self.c, 0, self.binary)
                with self.assertRaisesRegex(RuntimeError, 'fixture'):
                    self.invoke(p)
                self.assertEqual(os.environ.get(VARIABLE), previous)

    def test_missing_trace_preserves_abnormal_solver_record(self):
        p = self.protocol(lambda *args: dict(exit_code=137, timed_out=False))
        install_observer(p, self.c, 0, self.binary)
        result = self.invoke(p)
        self.assertEqual(result['exit_code'], 137)
        self.assertEqual(result['memory_trace'], dict(missing=True))

    def test_existing_evidence_is_not_overwritten(self):
        path = self.c / 'logs/000.slot-memory.tsv'
        path.write_text('old evidence')
        p = self.protocol(lambda *args: self.fail('must not run'))
        install_observer(p, self.c, 0, self.binary)
        with self.assertRaises(FileExistsError):
            self.invoke(p)
        self.assertEqual(path.read_text(), 'old evidence')

    def test_invalid_task_rejected(self):
        p = self.protocol(lambda *args: None)
        for task in (-1, '../0'):
            with self.assertRaises(ValueError):
                install_observer(p, self.c, task, self.binary)


if __name__ == '__main__':
    unittest.main()
