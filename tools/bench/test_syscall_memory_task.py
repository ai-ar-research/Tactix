import hashlib
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from syscall_memory_task import install_observer


class SyscallMemoryTaskTest(unittest.TestCase):
    def fixture(self, arm='candidate', write=True, result=None):
        tmp = tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / 'benchmarks/probes/scratch')
        self.addCleanup(tmp.cleanup)
        c = Path(tmp.name)
        (c / 'logs').mkdir()
        calls = []
        def measured(command, *args):
            calls.append((list(map(str, command)), args))
            if write and '-o' in command:
                Path(command[command.index('-o') + 1]).write_text('fixed syscall observation\n')
            return dict(result or {'exit_code': 20})
        protocol = SimpleNamespace(measured=measured,
            sha256=lambda p: hashlib.sha256(p.read_bytes()).hexdigest())
        for selected in ('candidate', 'previous'):
            install_observer(protocol, c, 0, c / selected, c / 'strace')
        return c, protocol, calls, [str(c / arm), 'input.cnf', 'proof.drat']

    def test_both_arms_and_outer_wrappers(self):
        for arm in ('candidate', 'previous'):
            with self.subTest(arm=arm):
                c, protocol, calls, command = self.fixture(arm)
                prefix = ['perf', 'stat', '--', 'taskset', '-c', '3']
                result = protocol.measured(prefix + command, 'out', 'err', 'metrics', 1200)
                self.assertEqual(len(calls), 1)
                words, args = calls[0]
                self.assertEqual(words[:len(prefix)], prefix)
                self.assertEqual(words[len(prefix)], str(c / 'strace'))
                self.assertEqual(words[-len(command):], command)
                self.assertEqual(args, ('out', 'err', 'metrics', 1200))
                self.assertEqual(result['exit_code'], 20)
                self.assertEqual(result['memory_syscalls']['path'], 'logs/000.memory-syscalls.txt')
                self.assertEqual(result['memory_syscalls']['bytes'], 26)

    def test_checker_bypass(self):
        _, protocol, calls, command = self.fixture('checker')
        result = protocol.measured(command, None, None, None, 16000)
        self.assertEqual(calls[0][0], command)
        self.assertNotIn('memory_syscalls', result)

    def test_missing_trace_retains_failure(self):
        _, protocol, _, command = self.fixture(write=False, result={'exit_code': 1})
        result = protocol.measured(command, None, None, None, 1200)
        self.assertEqual(result, {'exit_code': 1, 'memory_syscalls': {'missing': True}})

    def test_existing_trace_rejected(self):
        c, protocol, calls, command = self.fixture()
        (c / 'logs/000.memory-syscalls.txt').touch()
        with self.assertRaises(FileExistsError):
            protocol.measured(command, None, None, None, 1200)
        self.assertFalse(calls)

    def test_ambiguous_command_rejected(self):
        _, protocol, calls, command = self.fixture()
        with self.assertRaises(ValueError):
            protocol.measured(command + command, None, None, None, 1200)
        self.assertFalse(calls)

    def test_negative_task_rejected(self):
        with self.assertRaises(ValueError):
            install_observer(SimpleNamespace(), '.', -1, 'candidate', 'strace')


if __name__ == '__main__':
    unittest.main()
