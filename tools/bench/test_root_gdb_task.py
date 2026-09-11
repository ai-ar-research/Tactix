import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from root_gdb_task import install_observer


class RootGdbTaskTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.c = Path(self.temp.name)
        (self.c/'logs').mkdir()
        self.binary = str(self.c/'pins/candidate')

    def test_solver_only_and_restore(self):
        calls = []
        def measured(words, *args):
            calls.append(words)
            if self.binary in words:
                self.assertIn('--return-child-result', words)
                self.assertEqual(words[0:2], ['taskset', '-c'])
                Path(os.environ['TACTIX_GDB_OUTPUT']).write_text('finished')
            return {}
        protocol = SimpleNamespace(measured=measured, sha256=lambda p:'hash')
        install_observer(protocol, self.c, 0, self.binary)
        with patch.dict(os.environ, {'TACTIX_GDB_CONFIG':'prior'}):
            result = protocol.measured(['taskset', '-c', '1', self.binary, 'input'])
            self.assertEqual(os.environ['TACTIX_GDB_CONFIG'], 'prior')
            self.assertNotIn('TACTIX_GDB_OUTPUT', os.environ)
        self.assertEqual(result['root_gdb']['bytes'], 8)
        self.assertEqual(protocol.measured(['checker', 'input']), {})
        self.assertEqual(calls[-1], ['checker', 'input'])

    def test_missing(self):
        protocol = SimpleNamespace(measured=lambda *args:{})
        install_observer(protocol, self.c, 0, self.binary)
        self.assertEqual(protocol.measured([self.binary])['root_gdb'], dict(missing=True))

    def test_failure_restores(self):
        def fail(*args):
            raise RuntimeError('measurement failed')
        protocol = SimpleNamespace(measured=fail)
        install_observer(protocol, self.c, 0, self.binary)
        with patch.dict(os.environ, {}, clear=True):
            with self.assertRaises(RuntimeError):
                protocol.measured([self.binary])
            self.assertEqual(dict(os.environ), {})

    def test_invalid_and_existing(self):
        protocol = SimpleNamespace(measured=lambda *args:{})
        with self.assertRaises(ValueError):
            install_observer(protocol, self.c, -1, self.binary)
        install_observer(protocol, self.c, 0, self.binary)
        with self.assertRaises(ValueError):
            protocol.measured([self.binary, self.binary])
        p = self.c/'logs/000.root-gdb.jsonl'
        p.write_text('preserve')
        with self.assertRaises(FileExistsError):
            protocol.measured([self.binary])
        self.assertEqual(p.read_text(), 'preserve')


if __name__ == '__main__':
    unittest.main()
