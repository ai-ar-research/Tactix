import hashlib
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from proc_memory_task import FIELDS, install_proc_observer, process_start, sample_process, status_values


class ProcMemoryTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / 'benchmarks/probes/scratch')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'logs').mkdir()
        self.raw = '\n'.join(f'{key}:\t{100+i} kB' for i, key in enumerate(FIELDS))

    def test_status_exact_units_and_completeness(self):
        self.assertEqual(status_values(self.raw)['VmHWM'], 100)
        for raw in (self.raw.replace(' kB', ' MB'), self.raw + '\nVmHWM: 10 kB',
                    self.raw.replace('VmRSS', 'Unknown'), ''):
            with self.assertRaises(ValueError):
                status_values(raw)

    def test_process_identity_and_snapshot(self):
        base = self.root / '12'
        base.mkdir()
        stat = '12 (name with ) brackets) S ' + ' '.join(['0'] * 18 + ['77', '0'])
        self.assertEqual(process_start(stat), 77)
        (base / 'stat').write_text(stat)
        (base / 'status').write_text(self.raw)
        (base / 'smaps').write_text('raw map evidence\n')
        binary = self.root / 'candidate'
        binary.touch()
        (base / 'exe').symlink_to(binary)
        row = sample_process(12, binary, self.root, expected_start=77, maps=True)
        self.assertEqual(row['starttime_ticks'], 77)
        self.assertEqual(row['smaps'], 'raw map evidence\n')
        self.assertEqual(row['status'], row['status_after_smaps'])
        with self.assertRaises(ValueError):
            sample_process(12, binary, self.root, expected_start=78)
        other = self.root / 'other'
        other.touch()
        with self.assertRaises(ValueError):
            sample_process(12, other, self.root)

    def protocol(self, result=None, error=None):
        calls = []
        def measured(command, *args):
            calls.append((command, args))
            if error:
                raise error
            return dict(result or {'exit_code': 20})
        protocol = SimpleNamespace(measured=measured,
            sha256=lambda p: hashlib.sha256(p.read_bytes()).hexdigest())
        for arm in ('candidate', 'previous'):
            install_proc_observer(protocol, self.root, 0, self.root / arm)
        return protocol, calls

    def test_both_arms_preserve_command_and_arguments(self):
        for arm in ('candidate', 'previous'):
            protocol, calls = self.protocol()
            def fake_monitor(trace, binary, output, stop):
                output.write_text(json.dumps({'fixed': True}) + '\n')
            with patch('proc_memory_task.monitor', fake_monitor):
                command = ['perf', '--', str(self.root / arm), 'input', 'proof']
                result = protocol.measured(command, 'stdout', 'stderr', 'time', 1200)
            self.assertEqual(calls, [(command, ('stdout', 'stderr', 'time', 1200))])
            self.assertEqual(result['exit_code'], 20)
            self.assertEqual(result['proc_memory']['errors'], [])
            (self.root / 'logs/000.proc-memory.jsonl').unlink()

    def test_checker_bypass(self):
        protocol, calls = self.protocol()
        with patch('proc_memory_task.monitor') as monitor:
            result = protocol.measured(['checker'], 1)
        monitor.assert_not_called()
        self.assertEqual(result, {'exit_code': 20})

    def test_observer_failure_keeps_solver_failure(self):
        protocol, _ = self.protocol(result={'exit_code': 1})
        with patch('proc_memory_task.monitor', side_effect=OSError('fixture')):
            result = protocol.measured([str(self.root / 'candidate')])
        self.assertEqual(result['exit_code'], 1)
        self.assertTrue(result['proc_memory']['missing'])
        self.assertEqual(len(result['proc_memory']['errors']), 1)

    def test_solver_exception_stops_monitor(self):
        protocol, _ = self.protocol(error=RuntimeError('solver wrapper failure'))
        stops = []
        def fake_monitor(trace, binary, output, stop):
            stops.append(stop)
            stop.wait(1)
        with patch('proc_memory_task.monitor', fake_monitor), self.assertRaises(RuntimeError):
            protocol.measured([str(self.root / 'candidate')])
        self.assertTrue(stops[0].is_set())

    def test_existing_output_and_duplicate_binary_rejected(self):
        protocol, calls = self.protocol()
        binary = str(self.root / 'candidate')
        with self.assertRaises(ValueError):
            protocol.measured([binary, binary])
        (self.root / 'logs/000.proc-memory.jsonl').touch()
        with self.assertRaises(FileExistsError):
            protocol.measured([binary])
        self.assertFalse(calls)

    def test_negative_task(self):
        with self.assertRaises(ValueError):
            install_proc_observer(SimpleNamespace(), self.root, -1, 'candidate')


if __name__ == '__main__':
    unittest.main()
