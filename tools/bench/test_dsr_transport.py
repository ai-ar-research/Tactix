"""Differential transport, immutable packaging and anonymous-runner lifecycle."""
import hashlib
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import proof_oracle as reference
from anonymous_scratch import AnonymousScratch
from runtime_task import ProofPipelineDeadline, proof_pipeline_deadline

HELPER = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None


def encode(value):
    out = bytearray()
    while value >= 128:
        out.append((value & 127) | 128)
        value >>= 7
    out.append(value)
    return bytes(out)


class TransportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        source = Path(reference.__file__)
        shutil.copyfile(source, self.root / 'proof_oracle.py')
        reference.pin_dsr_transport(HELPER, self.root)
        spec = importlib.util.spec_from_file_location('pinned_transport_oracle', self.root / source.name)
        self.oracle = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.oracle)
        self.source, self.dest = self.root / 'input.dsr', self.root / 'output.drat'

    def reference_status(self):
        kind = first = None
        try:
            for event, item in reference._dsr_binary_events(self.source):
                if event == 'start':
                    kind, first = item, None
                elif event == 'token':
                    if first is None:
                        first = item
                    elif kind == 'a' and item == first:
                        return 2
        except reference.DSRFormatError:
            return 3
        return 0

    def test_grammar_differential(self):
        cases = [b'', b'\1\0', b'\2\0', b'\1', b'\1\x80', b'\3\0',
                 b'\1\x80\x80\x80\x80\x80\0', b'\1\x80\x80\x80\x80\x10\0',
                 b'\1\x80\0', b'\1\x82\0\0', b'\1\2\2\0', b'\2\2\2\0',
                 b'\1\2\4\2\0', b'\1\2\2\0\3', b'\1\2\0a\4\0d\4\0\2\2\0']
        values = [1, 2, 3, 127, 128, 129, 16383, 16384, 2097151, 2097152,
                  268435455, 268435456, 4294967295]
        for tag in (1, 2, ord('a'), ord('d')):
            for value in values:
                cases += [bytes([tag])+encode(value)+b'\0',
                          bytes([tag])+encode(value)*2+b'\0']
        for offset in range(6):
            pad = (1 << 20) - offset
            cases += [b'\1\4'+b'\2'*(pad-1)+encode(4294967295)+b'\0',
                      b'\2\4\0'*(pad//3)+b'\1'+encode(4294967295)+b'\0']
        self.assertEqual(len(cases), 131)
        for index, case in enumerate(cases):
            with self.subTest(case=index):
                self.source.write_bytes(case)
                expected = self.reference_status()
                for mode in ('scan', 'convert'):
                    self.dest.write_bytes(b'KEEP')
                    cmd = [str(HELPER), mode, str(self.source)]
                    if mode == 'convert':
                        cmd.append(str(self.dest))
                    result = subprocess.run(cmd, capture_output=True)
                    self.assertEqual(result.returncode, expected, result.stderr)
                    if mode == 'convert':
                        if expected:
                            self.assertEqual(self.dest.read_bytes(), b'KEEP')
                        else:
                            reference.dsr_proof_as_drat(self.source, self.root / 'reference.drat')
                            self.assertEqual(self.dest.read_bytes(), (self.root / 'reference.drat').read_bytes())
                self.assertEqual(list(self.root.glob('*.dsr-transport-*')), [])

    def test_python_integration_and_fallback(self):
        self.source.write_bytes(b'\1\2\4\0\2\4\0')
        self.assertFalse(self.oracle.dsr_proof_has_witness(self.source))
        self.assertTrue(self.oracle.dsr_proof_as_drat(self.source, self.dest))
        self.assertEqual(self.dest.read_bytes(), b'a\2\4\0d\4\0')
        (self.root / 'dsr-transport').unlink()
        (self.root / 'dsr-transport.sha256').unlink()
        self.assertTrue(self.oracle.dsr_proof_as_drat(self.source, self.dest))
        self.assertEqual(self.dest.read_bytes(), b'a\2\4\0d\4\0')
        self.source.write_text('1 -2 0\nd -2 0\n')
        self.assertTrue(self.oracle.dsr_proof_as_drat(self.source, self.dest))
        self.assertEqual(self.dest.read_bytes(), self.source.read_bytes())

    def test_corrupt_or_missing_pin_never_silently_falls_back(self):
        self.source.write_bytes(b'\1\0')
        helper = self.root / 'dsr-transport'
        helper.chmod(0o750)
        helper.write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'pinned DSR transport'):
            self.oracle.dsr_proof_has_witness(self.source)
        helper.unlink()
        with self.assertRaisesRegex(RuntimeError, 'pinned DSR transport'):
            self.oracle.dsr_proof_has_witness(self.source)

    def test_format_and_io_failures_are_distinct(self):
        self.source.write_bytes(b'\1\x80')
        self.dest.write_bytes(b'KEEP')
        with self.assertRaises(self.oracle.DSRFormatError):
            self.oracle.dsr_proof_as_drat(self.source, self.dest)
        self.assertEqual(self.dest.read_bytes(), b'KEEP')
        with self.assertRaises(OSError):
            self.oracle._native_dsr_transport('scan', self.root / 'absent')
        self.source.write_bytes(b'\1\0')
        with self.assertRaises(OSError):
            self.oracle.dsr_proof_as_drat(self.source, self.root / 'absent/target')
        with self.assertRaises(OSError):
            self.oracle.dsr_proof_as_drat(self.source, self.source)
        self.assertEqual(self.source.read_bytes(), b'\1\0')

    def test_actual_anonymous_scratch_lifecycle(self):
        scratch = AnonymousScratch(self.root)
        self.addCleanup(scratch.close)
        source, target = scratch / 'proof.dsr', scratch / 'proof.drat'
        source.write_bytes(b'\1\2\0\2\2\0')
        target.write_bytes(b'KEEP')
        before = target.stat().st_ino
        self.assertTrue(self.oracle.dsr_proof_as_drat(source, target))
        self.assertEqual(target.read_bytes(), b'a\2\0d\2\0')
        self.assertEqual(target.stat().st_ino, before)
        self.assertEqual(target.stat().st_nlink, 0)
        self.assertFalse(any('dsr-transport-' in p.name for p in self.root.iterdir()))
        for malformed in (b'\1\x80', b'\1\2\2\0'):
            source.write_bytes(malformed)
            target.write_bytes(b'KEEP')
            if malformed.endswith(b'\x80'):
                with self.assertRaises(self.oracle.DSRFormatError):
                    self.oracle.dsr_proof_as_drat(source, target)
            else:
                self.assertFalse(self.oracle.dsr_proof_as_drat(source, target))
            self.assertEqual(target.read_bytes(), b'KEEP')
        scratch.close()
        self.assertFalse(source.exists())
        self.assertFalse(target.exists())

    def test_independent_oracles_through_anonymous_runtime(self):
        import runtime_task
        root = Path(__file__).resolve().parents[2]
        checkers = Path(os.environ.get('DSR_TRANSPORT_CHECKERS', root / 'tools/checkers/bin'))
        for name in ('drat-trim', 'cake_lpr'):
            if not (checkers / name).is_file():
                self.skipTest('independent checker unavailable: ' + name)
        campaign = self.root / 'campaign'
        (campaign / 'pins').mkdir(parents=True)
        for name in ('drat-trim', 'cake_lpr'):
            shutil.copy2(checkers / name, campaign / 'pins' / name)
        for good in (True, False):
            # Released anonymous names are deliberately tombstoned for one
            # task.  Each runtime invocation owns a fresh scratch namespace.
            scratch = AnonymousScratch(self.root)
            self.addCleanup(scratch.close)
            cnf, proof = scratch / 'original.cnf', scratch / 'proof.dsr'
            proof.write_bytes(b'\1\0')
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n' if good else 'p cnf 1 1\n1 0\n')
            with patch.object(runtime_task, 'certificate_route', self.oracle.certificate_route), \
                 patch.object(runtime_task, 'dsr_proof_as_drat', self.oracle.dsr_proof_as_drat):
                outcome = runtime_task.check_unsat_proof(
                    campaign, 'dsr', cnf, proof, scratch, 60, 'dsr-cake_lpr')
            self.assertEqual(outcome['result'] == 'verified', good, outcome)
            self.assertEqual(outcome['stages'][0]['name'], 'drat-trim')
            if good:
                self.assertEqual(outcome['stages'][-1]['name'], 'cake_lpr')
                self.assertTrue(all(stage['result'] == 'verified' for stage in outcome['stages']))
            else:
                # The runner may classify an empty produced LRAT as error;
                # require rejection by the actual checker, never just any error.
                stage = outcome['stages'][0]
                self.assertEqual(reference.proof_check_status(
                    'drat-trim', stage['exit_code'], stage['output_tail'], ''), 'rejected')
        self.assertEqual(list(campaign.glob('*.dsr-transport-*')), [])

    def test_real_signal_kills_leave_no_named_partial_output(self):
        # Bounded form of the independent owner's SIGXFSZ/SIGKILL reproducer.
        # Kill after output bytes exist, not merely during the validation pass.
        import resource
        import signal
        import threading
        import time
        block = b'\1\2\4\0' * (1 << 18)
        with self.source.open('wb') as stream:
            for _ in range(128):
                stream.write(block)
        original = subprocess.Popen
        for managed, cause in ((False, 'xfsz'), (False, 'kill'),
                               (True, 'xfsz'), (True, 'kill'), (True, 'deadline')):
            with self.subTest(managed=managed, cause=cause):
                self.dest.write_bytes(b'KEEP')
                processes, workers, observed, failures = [], [], [], []
                def stop_during_write(process):
                    try:
                        until = time.monotonic() + 20
                        while time.monotonic() < until and process.poll() is None:
                            paths = (list(self.root.glob('.output.drat.dsr-transport-*/output'))
                                     if managed else list(Path(f'/proc/{process.pid}/fd').glob('*')))
                            for path in paths:
                                try:
                                    info = path.stat()
                                except FileNotFoundError:
                                    continue
                                if info.st_size < 65536 or (not managed and info.st_nlink != 0):
                                    continue
                                observed.append(True)
                                if cause == 'deadline':
                                    os.kill(os.getpid(), signal.SIGALRM)
                                else:
                                    process.kill()
                                return
                            time.sleep(0.001)
                        failures.append('did not observe the conversion write phase')
                        if process.poll() is None:
                            process.kill()
                    except BaseException as error:
                        failures.append(repr(error))
                        if process.poll() is None:
                            process.kill()
                def tracked(*args, **kwargs):
                    if cause == 'xfsz':
                        kwargs['preexec_fn'] = lambda: resource.setrlimit(
                            resource.RLIMIT_FSIZE, (65536, resource.RLIM_INFINITY))
                    process = original(*args, **kwargs)
                    processes.append(process)
                    if cause != 'xfsz':
                        worker = threading.Thread(target=stop_during_write, args=(process,))
                        workers.append(worker)
                        worker.start()
                    return process
                with patch.object(subprocess, 'Popen', side_effect=tracked):
                    if managed:
                        if cause == 'deadline':
                            with self.assertRaises(ProofPipelineDeadline):
                                with proof_pipeline_deadline(30):
                                    self.oracle.dsr_proof_as_drat(self.source, self.dest)
                        else:
                            with self.assertRaises(OSError):
                                self.oracle.dsr_proof_as_drat(self.source, self.dest)
                    else:
                        subprocess.run([str(HELPER), 'convert', str(self.source), str(self.dest)],
                                       capture_output=True, timeout=30)
                for worker in workers:
                    worker.join(timeout=5)
                    self.assertFalse(worker.is_alive())
                self.assertEqual(failures, [])
                self.assertEqual(len(processes), 1)
                expected = -signal.SIGXFSZ if cause == 'xfsz' else -signal.SIGKILL
                self.assertEqual(processes[0].returncode, expected)
                if cause != 'xfsz':
                    self.assertTrue(observed)
                with self.assertRaises(ProcessLookupError):
                    os.kill(processes[0].pid, 0)
                self.assertEqual(self.dest.read_bytes(), b'KEEP')
                self.assertFalse(any('dsr-transport-' in p.name for p in self.root.iterdir()))

    def test_shared_deadline_reaps_converter(self):
        helper = self.root / 'dsr-transport'
        helper.chmod(0o750)
        helper.write_text('#!/usr/bin/env python3\nimport time\ntime.sleep(30)\n')
        (self.root / 'dsr-transport.sha256').write_text(hashlib.sha256(helper.read_bytes()).hexdigest())
        self.source.write_bytes(b'\1\0')
        processes = []
        original = subprocess.Popen
        def tracked(*args, **kwargs):
            process = original(*args, **kwargs)
            processes.append(process)
            return process
        with patch.object(subprocess, 'Popen', side_effect=tracked):
            with self.assertRaises(ProofPipelineDeadline):
                with proof_pipeline_deadline(0.2):
                    self.oracle.dsr_proof_as_drat(self.source, self.dest)
        self.assertEqual(len(processes), 1)
        self.assertIsNotNone(processes[0].poll())
        with self.assertRaises(ProcessLookupError):
            os.kill(processes[0].pid, 0)

    def test_deadline_after_staging_acquisition_is_deferred_through_cleanup(self):
        import signal
        self.source.write_bytes(b'\1\4\6\0' * 4096)
        self.oracle.dsr_proof_as_drat(self.source, self.dest)
        full = self.dest.read_bytes()
        self.dest.write_bytes(b'KEEP')
        original = tempfile.mkdtemp

        def interrupt_after_create(*args, **kwargs):
            directory = original(*args, **kwargs)
            os.kill(os.getpid(), signal.SIGALRM)
            return directory

        with patch.object(tempfile, 'mkdtemp', side_effect=interrupt_after_create):
            with self.assertRaises(ProofPipelineDeadline):
                with proof_pipeline_deadline(30):
                    self.oracle.dsr_proof_as_drat(self.source, self.dest)
        self.assertFalse(any('dsr-transport-' in path.name
                             for path in self.root.iterdir()))
        self.assertIn(self.dest.read_bytes(), (b'KEEP', full))

    def test_deadline_at_cleanup_entry_is_deferred_until_namespace_removed(self):
        import signal
        self.source.write_bytes(b'\1\4\6\0' * 4096)
        self.dest.write_bytes(b'KEEP')
        original = self.oracle._remove_transport_staging

        def interrupt_then_remove(directory):
            os.kill(os.getpid(), signal.SIGALRM)
            return original(directory)

        with patch.object(self.oracle, '_remove_transport_staging',
                          side_effect=interrupt_then_remove):
            with self.assertRaises(ProofPipelineDeadline):
                with proof_pipeline_deadline(30):
                    self.oracle.dsr_proof_as_drat(self.source, self.dest)
        self.assertFalse(any('dsr-transport-' in path.name
                             for path in self.root.iterdir()))

    def test_binary_probe_does_not_swallow_pipeline_deadline(self):
        deadline = ProofPipelineDeadline('deadline during binary probe')
        with patch('builtins.open', side_effect=deadline):
            with self.assertRaises(ProofPipelineDeadline) as raised:
                self.oracle.dsr_proof_is_binary(self.source)
        self.assertIs(raised.exception, deadline)

    def test_binary_probe_declines_actual_filesystem_error(self):
        self.assertFalse(self.oracle.dsr_proof_is_binary(
            self.root / 'missing-proof.dsr'))

    def test_nfs_staging_cleanup_retries_only_transient_errors(self):
        import errno
        import time
        directory = self.root / 'owned-staging'
        directory.mkdir()
        (directory / '.nfs-delayed-close').write_bytes(b'partial')
        unrelated = self.root / 'other-conversion'
        unrelated.mkdir()
        (unrelated / 'output').write_bytes(b'KEEP')
        remove = shutil.rmtree
        calls = []
        def delayed(path):
            calls.append(Path(path))
            if len(calls) <= 3:
                raise OSError((errno.EBUSY, errno.ENOTEMPTY, errno.ENOENT)[len(calls) - 1],
                              'NFS delayed close', str(path))
            remove(path)
        with patch.object(shutil, 'rmtree', side_effect=delayed), \
                patch.object(time, 'sleep') as sleep:
            self.oracle._remove_transport_staging(directory)
        self.assertEqual(calls, [directory] * 4)
        self.assertEqual([c.args[0] for c in sleep.call_args_list], [0.05, 0.1, 0.2])
        self.assertFalse(directory.exists())
        self.assertEqual((unrelated / 'output').read_bytes(), b'KEEP')
        with patch.object(shutil, 'rmtree', side_effect=PermissionError(
                errno.EACCES, 'denied')) as blocked, patch.object(time, 'sleep') as sleep:
            with self.assertRaises(PermissionError):
                self.oracle._remove_transport_staging(unrelated)
            self.assertEqual(blocked.call_count, 1)
            sleep.assert_not_called()

    def test_terminal_cleanup_preserves_pipeline_deadline(self):
        import errno
        self.source.write_bytes(b'\1\2\4\0')
        self.dest.write_bytes(b'KEEP')
        deadline = ProofPipelineDeadline('deadline')
        cleanup = OSError(errno.ENOTEMPTY, 'persistent NFS failure')
        remove = self.oracle._remove_transport_staging
        owned = []
        def fail_cleanup(directory):
            owned.append(directory)
            raise cleanup
        try:
            with patch.object(subprocess, 'Popen', side_effect=deadline), \
                    patch.object(self.oracle, '_remove_transport_staging',
                                 side_effect=fail_cleanup):
                with self.assertRaises(ProofPipelineDeadline) as raised:
                    self.oracle.dsr_proof_as_drat(self.source, self.dest)
            self.assertIs(raised.exception, deadline)
            self.assertIs(raised.exception.__cause__, cleanup)
            self.assertEqual(self.dest.read_bytes(), b'KEEP')
        finally:
            for directory in owned:
                remove(directory)

    def test_cleanup_defers_deadline_until_staging_is_removed(self):
        import errno
        import time
        directory = self.root / 'deadline-staging'
        directory.mkdir()
        (directory / '.nfs-delayed-close').write_bytes(b'partial')
        deadline = ProofPipelineDeadline('deadline during cleanup')
        remove = shutil.rmtree
        calls = 0

        def delayed(path):
            nonlocal calls
            calls += 1
            if calls == 1:
                raise OSError(errno.ENOTEMPTY, 'NFS delayed close', str(path))
            remove(path)

        with patch.object(shutil, 'rmtree', side_effect=delayed), \
                patch.object(time, 'sleep', side_effect=deadline):
            with self.assertRaises(ProofPipelineDeadline) as raised:
                self.oracle._remove_transport_staging(directory)
        self.assertIs(raised.exception, deadline)
        self.assertEqual(calls, 2)
        self.assertFalse(directory.exists())

    def test_transport_deadline_during_cleanup_leaves_no_staging(self):
        import errno
        import signal
        import time
        self.source.write_bytes(b'\1\0')
        self.dest.write_bytes(b'KEEP')
        deadline = ProofPipelineDeadline('deadline during cleanup')
        remove = shutil.rmtree
        injected = False

        def delayed(path):
            nonlocal injected
            if not injected and '.dsr-transport-' in str(path):
                injected = True
                raise OSError(errno.ENOTEMPTY, 'NFS delayed close', str(path))
            remove(path)

        with patch.object(shutil, 'rmtree', side_effect=delayed), \
                patch.object(time, 'sleep', side_effect=deadline):
            with self.assertRaises(ProofPipelineDeadline) as raised:
                self.oracle.dsr_proof_as_drat(self.source, self.dest)
        self.assertIs(raised.exception, deadline)
        self.assertTrue(injected)
        self.assertIn(self.dest.read_bytes(), (b'KEEP', b'a\0'))
        self.assertFalse(any('dsr-transport-' in p.name
                             for p in self.root.iterdir()))

    def test_cleanup_defers_deadline_during_enoent_stat(self):
        import errno
        directory = self.root / 'enoent-stat-staging'
        directory.mkdir()
        (directory / '.nfs-delayed-close').write_bytes(b'partial')
        deadline = ProofPipelineDeadline('deadline during ENOENT stat')
        remove = shutil.rmtree
        calls = 0

        def missing_child(path):
            nonlocal calls
            calls += 1
            if calls == 1:
                raise OSError(errno.ENOENT, 'NFS child disappeared', str(path))
            remove(path)

        with patch.object(shutil, 'rmtree', side_effect=missing_child), \
                patch.object(Path, 'exists', side_effect=deadline):
            with self.assertRaises(ProofPipelineDeadline) as raised:
                self.oracle._remove_transport_staging(directory)
        self.assertIs(raised.exception, deadline)
        self.assertEqual(calls, 2)
        self.assertFalse(directory.exists())

    def test_deadline_remains_primary_after_bounded_cleanup_failure(self):
        import errno
        import time
        directory = self.root / 'persistent-staging'
        directory.mkdir()
        deadline = ProofPipelineDeadline('deadline during cleanup')
        sleeps = 0

        def interrupted_once(_delay):
            nonlocal sleeps
            sleeps += 1
            if sleeps == 1:
                raise deadline

        cleanup = OSError(errno.ENOTEMPTY, 'persistent NFS failure')
        with patch.object(shutil, 'rmtree', side_effect=cleanup) as remove, \
                patch.object(time, 'sleep', side_effect=interrupted_once):
            with self.assertRaises(ProofPipelineDeadline) as raised:
                self.oracle._remove_transport_staging(directory)
        self.assertIs(raised.exception, deadline)
        self.assertIs(raised.exception.__cause__, cleanup)
        self.assertEqual(remove.call_count, 20)
        self.assertEqual(sleeps, 19)

    def test_repeated_cleanup_interrupts_are_bounded(self):
        directory = self.root / 'repeated-interrupt-staging'
        directory.mkdir()
        first = KeyboardInterrupt('first cleanup interrupt')
        interrupts = [first] + [KeyboardInterrupt('later cleanup interrupt')
                                for _ in range(19)]
        with patch.object(shutil, 'rmtree', side_effect=interrupts) as remove:
            with self.assertRaises(KeyboardInterrupt) as raised:
                self.oracle._remove_transport_staging(directory)
        self.assertIs(raised.exception, first)
        self.assertIsInstance(raised.exception.__cause__, OSError)
        self.assertEqual(remove.call_count, 20)

    def test_nfs_staging_cleanup_has_bounded_retries(self):
        import errno
        import time
        with patch.object(shutil, 'rmtree', side_effect=OSError(
                errno.ENOTEMPTY, 'persistent NFS failure')) as remove, \
                patch.object(time, 'sleep') as sleep:
            with self.assertRaises(OSError):
                self.oracle._remove_transport_staging(self.root / 'owned-only')
            self.assertEqual(remove.call_count, 20)
            self.assertEqual(sleep.call_count, 19)
            self.assertLessEqual(max(c.args[0] for c in sleep.call_args_list), 2.0)


if __name__ == '__main__':
    if HELPER is None or not HELPER.is_file():
        raise SystemExit('usage: test_dsr_transport.py /path/to/dsr-transport')
    unittest.main()
