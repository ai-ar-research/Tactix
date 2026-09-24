from pathlib import Path
import os
import tempfile
import unittest

from proof_oracle import bridge_trivial_input_binary_lsr, proof_check_status


class ProofOracleTests(unittest.TestCase):
    def test_captured_wait_chunks_preserve_output_and_exit_status(self):
        import proof_oracle as oracle
        import subprocess
        import sys
        from unittest.mock import patch
        calls = []
        communicate = subprocess.Popen.communicate
        def observed(process, *args, **kwargs):
            calls.append(kwargs.get('timeout'))
            return communicate(process, *args, **kwargs)
        command = [sys.executable, '-c',
                   'import os,sys,time; print("x"*100000,flush=True); '
                   'print("before",file=sys.stderr,flush=True); time.sleep(.2); '
                   'print(os.environ["TACTIX_WAIT_TEST"]); sys.exit(7)']
        with patch.object(oracle, 'MAX_CAPTURE_WAIT_SECONDS', .04), \
             patch.object(subprocess.Popen, 'communicate', observed):
            result = oracle.run_captured(command, timeout=3,
                                         env={**os.environ, 'TACTIX_WAIT_TEST':'after'})
        self.assertEqual(result.returncode, 7)
        self.assertEqual(result.stdout, 'x'*100000+'\nafter\n')
        self.assertEqual(result.stderr, 'before\n')
        self.assertGreater(len(calls), 1)
        self.assertTrue(all(0 <= delay <= .04 for delay in calls), calls)

    def test_captured_wait_timeout_kills_reaps_and_preserves_output(self):
        import proof_oracle as oracle
        import subprocess
        import sys
        import time
        from unittest.mock import patch
        children = []
        popen = subprocess.Popen
        def start(*args, **kwargs):
            process = popen(*args, **kwargs)
            children.append(process)
            return process
        started = time.monotonic()
        with patch.object(oracle, 'MAX_CAPTURE_WAIT_SECONDS', .04), \
             patch('subprocess.Popen', side_effect=start), \
             self.assertRaises(subprocess.TimeoutExpired) as caught:
            oracle.run_captured([sys.executable, '-c',
                'import sys,time; print("partial",flush=True); '
                'print("err",file=sys.stderr,flush=True); time.sleep(60)'], timeout=.4)
        self.assertEqual(caught.exception.timeout, .4)
        self.assertEqual(caught.exception.stdout, 'partial\n')
        self.assertEqual(caught.exception.stderr, 'err\n')
        self.assertLess(time.monotonic() - started, 5)
        self.assertEqual(len(children), 1)
        self.assertIsNotNone(children[0].returncode)
        with self.assertRaises(ChildProcessError):
            os.waitpid(children[0].pid, os.WNOHANG)

    def test_captured_wait_preserves_normal_path_and_missing_command_errors(self):
        import proof_oracle as oracle
        import subprocess
        from unittest.mock import patch
        for timeout in (None, .1, 57600):
            with patch('subprocess.run', return_value='delegated') as run:
                self.assertEqual(oracle.run_captured(['checker'], timeout, env={}), 'delegated')
                run.assert_called_once_with(['checker'], capture_output=True,
                    text=True, errors='replace', timeout=timeout, env={})
        with self.assertRaises(FileNotFoundError):
            oracle.run_captured(['/no-such-tactix-checker'], 2147483647)

    def test_large_allowance_reaches_real_routed_and_tier_checkers(self):
        import importlib.util
        import proof_oracle as oracle
        from unittest.mock import patch
        root = Path(__file__).resolve().parents[2]
        pins = root/'benchmarks/runs/20260923-am341-unwatch/pins'
        spec = importlib.util.spec_from_file_location('large_wait_runner', root/'tools/bench/run.py')
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'in.cnf', work/'certificate.drat'
            cnf.write_text('p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n')
            proof.write_text('1 0\n0\n')
            for budget in (2147483.647, 2147484, 2147483647):
                with self.subTest(budget=budget):
                    checked = oracle.check_routed(*(pins/name for name in (
                        'dsr-trim','dsr-compress','srcheck','drat-trim','cake_lpr')),
                        cnf, proof, work, timeout=budget)
                    self.assertTrue(checked[0], checked)
                    with patch.object(runner, 'DRAT_TRIM', str(pins/'drat-trim')):
                        checked = runner.check_proof('drat', str(cnf), str(proof), budget)
                    self.assertTrue(checked[0], checked)

    def test_duplicate_restatement_preserves_noop_and_input_paths(self):
        from proof_oracle import canonical_checker_cnf, restate_duplicate_input_lrat
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, checker, lrat, lifted = (work/name for name in ('in.cnf','check.cnf','p.lrat','lift.lrat'))
            for text, duplicates in (('p cnf 1 2\n1 0\n-1 0\n', 0),
                                     ('p cnf 1 3\n1 1 -1 0\n1 0\n-1 0\n', 1)):
                cnf.write_text(text)
                spelling = canonical_checker_cnf(cnf, checker)
                self.assertEqual(spelling['duplicate_literal_clauses'], duplicates)
                lrat.write_text('4 0 2 3 0\n')
                self.assertEqual(restate_duplicate_input_lrat(checker, spelling, lrat, lifted), (lrat, 0))
                self.assertEqual(lrat.read_text(), '4 0 2 3 0\n')
            cnf.write_text('p cnf 1 2\n1 1 0\n-1 0\n')
            spelling = canonical_checker_cnf(cnf, checker)
            for target in (checker,lrat):
                before=target.read_bytes()
                with self.assertRaises(ValueError):
                    restate_duplicate_input_lrat(checker, spelling, lrat, target)
                self.assertEqual(target.read_bytes(),before)
            lrat.write_text('3 0 1 2')
            with self.assertRaises(ValueError):
                restate_duplicate_input_lrat(checker, spelling, lrat, lifted)

    def test_routing_does_not_misclassify_missing_files_as_bad_proofs(self):
        import proof_oracle as oracle
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            with self.assertRaises(OSError):
                oracle.check_routed(None, None, None, None, None,
                                    work/'input.cnf', work/'absent.dsr', work)

    def test_dsr_tabs_and_binary_token_boundaries(self):
        import proof_oracle as oracle
        with tempfile.TemporaryDirectory() as directory:
            proof = Path(directory)/'proof.dsr'
            proof.write_bytes(b'1\t2\t0\r\nd\t1 2 0\n0\n')
            self.assertFalse(oracle.dsr_proof_is_binary(proof))
            self.assertEqual(oracle.certificate_route(proof), 'drat')
            for value in (2, 127, 128, 16383, 16384, (1 << 32) - 1):
                encoded, rest = bytearray(), value
                while rest >= 128:
                    encoded.append((rest & 127) | 128)
                    rest >>= 7
                encoded.append(rest)
                proof.write_bytes(b'\x01' + encoded + b'\x00')
                self.assertEqual(list(oracle._dsr_lines(proof, True)), [('a', [value])])
                self.assertTrue(oracle.dsr_proof_as_drat(proof, Path(directory)/'out.drat'))
                self.assertEqual((Path(directory)/'out.drat').read_bytes(), b'a' + encoded + b'\x00')

    def test_malformed_dsr_is_a_routing_rejection(self):
        import proof_oracle as oracle
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'input.cnf', work/'proof.dsr'
            cnf.write_text('p cnf 1 1\n1 0\n')
            for data in (b'\x01', b'\x01\x80', b'\x02\x02', b'\x03\x00',
                         b'\x01\x80\x80\x80\x80\x80\x00', b'1', b'd\n', b'--1 0\n'):
                with self.subTest(data=data), patch('subprocess.run') as run:
                    proof.write_bytes(data)
                    result = oracle.check_routed(None, None, None, None, None,
                                                 cnf, proof, work)
                    self.assertFalse(result[0])
                    self.assertEqual(result[1], 'route')
                    self.assertIn('DSR', result[2])
                    run.assert_not_called()
                    with self.assertRaises(ValueError):
                        oracle.dsr_proof_as_drat(proof, work/'out.drat')
                    self.assertFalse((work/'out.drat').exists())

    def test_drat_internal_timeout_tracks_the_requested_window(self):
        import proof_oracle
        command = ['drat-trim', 'original.cnf', 'proof.drat', '-L', 'proof.lrat']
        for seconds, value in ((57600, '57600'), (0.5, '1'), (31.1, '32')):
            self.assertEqual(proof_oracle.drat_command_with_timeout(command, seconds),
                             command[:3] + ['-t', value] + command[3:])
        self.assertEqual(command, ['drat-trim', 'original.cnf', 'proof.drat', '-L', 'proof.lrat'])
        self.assertEqual(proof_oracle.drat_command_with_timeout(command, None), command)
        for invalid in (0, -1, float('inf'), float('nan'), True, 2147483648):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                proof_oracle.drat_command_with_timeout(command, invalid)

    def test_input_empty_lrat_bridge_is_narrow(self):
        from proof_oracle import bridge_trivial_input_lrat, canonical_checker_cnf
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, lrat = work/'input.cnf', work/'proof.lrat'
            cnf.write_text('p cnf 2 3\n1 2 0\n0\n-1 0\n')
            spelling = canonical_checker_cnf(cnf, work/'checker.cnf')
            success = 'c trivial UNSAT\ns VERIFIED\n'
            self.assertFalse(bridge_trivial_input_lrat(spelling, lrat, 1, success))
            self.assertFalse(lrat.exists(), 'must not fabricate a missing stage output')
            for original in ('', '4 0 1 0\n'):
                for code, output in ((0, success), (-9, success), (1, 's VERIFIED\n'),
                                     (1, success + 's NOT VERIFIED\n')):
                    lrat.write_text(original)
                    self.assertFalse(bridge_trivial_input_lrat(spelling, lrat, code, output))
                    self.assertEqual(lrat.read_text(), original)
            lrat.write_text('4 0 1 0\n')
            self.assertFalse(bridge_trivial_input_lrat(spelling, lrat, 1, success))
            self.assertEqual(lrat.read_text(), '4 0 1 0\n')
            lrat.write_text('')
            self.assertTrue(bridge_trivial_input_lrat(spelling, lrat, 1, success))
            self.assertEqual(lrat.read_text(), '4 0 2 0\n')
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            spelling = canonical_checker_cnf(cnf, work/'checker.cnf')
            lrat.write_text('')
            self.assertFalse(bridge_trivial_input_lrat(spelling, lrat, 1, success))
            self.assertEqual(lrat.read_text(), '')

    def test_exact_drat_success_with_carriage_return(self):
        self.assertEqual(proof_check_status('drat-trim', 0, '\rs VERIFIED\n', ''), 'verified')

    def test_exact_cake_success(self):
        self.assertEqual(proof_check_status('cake_lpr', 0, 's VERIFIED UNSAT\n', ''), 'verified')

    def test_exact_dsr_success(self):
        self.assertEqual(
            proof_check_status('dsr-trim', 0, 's VERIFIED UNSAT\n', ''),
            'verified')

    def test_dsr_rejection(self):
        self.assertEqual(
            proof_check_status('dsr-trim', 1, '', 'Error: invalid witness\n'),
            'rejected')

    def test_rejection_precedes_success(self):
        self.assertEqual(proof_check_status('drat-trim', 1, 's VERIFIED\ns NOT VERIFIED\n', ''), 'rejected')

    def test_drat_trivial_unsat_exit_one_is_source_confirmed_success(self):
        self.assertEqual(proof_check_status(
            'drat-trim', 1, 'c trivial UNSAT\ns VERIFIED\n', ''), 'verified')

    def test_other_drat_exit_one_is_not_rescued(self):
        self.assertEqual(proof_check_status(
            'drat-trim', 1, 's VERIFIED\n', ''), 'error')

    def test_cake_zero_exit_is_not_acceptance(self):
        self.assertEqual(proof_check_status('cake_lpr', 0, '', 'c Checking failed at line: 0. Reason: pivot must be non-zero\n'), 'rejected')

    def test_cake_incomplete(self):
        self.assertEqual(proof_check_status('cake_lpr', 0, '', 'c empty clause not derived at end of proof\n'), 'rejected')

    def test_missing_wrong_and_embedded_tokens(self):
        for checker, text in (('cake_lpr', ''), ('cake_lpr', 'c VERIFIED'),
                              ('cake_lpr', 's VERIFIED'), ('drat-trim', 's DERIVATION'),
                              ('drat-trim', 'prefix s VERIFIED'), ('drat-trim', 's VERIFIED UNSAT')):
            with self.subTest(checker=checker, text=text):
                self.assertEqual(proof_check_status(checker, 0, text, ''), 'error')

    def test_abnormal_exit(self):
        for checker in ('cake_lpr', 'dsr-trim'):
            for code in (1, -9):
                with self.subTest(checker=checker, code=code):
                    self.assertEqual(
                        proof_check_status(checker, code,
                                           's VERIFIED UNSAT\n', ''), 'error')

    def test_legacy_checker_cannot_be_selected(self):
        with self.assertRaises(ValueError):
            proof_check_status('lrat-check', 0, 'c VERIFIED\n', '')

    def test_binary_trivial_input_bridge(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cnf = root / 'input.cnf'
            lsr = root / 'proof.binary.lsr'
            cnf.write_text('p cnf 2 2\n1 2 0\n0\n')
            lsr.write_bytes(bytes([3, 6, 0, 0]))
            self.assertTrue(bridge_trivial_input_binary_lsr(cnf, lsr))
            self.assertEqual(lsr.read_bytes(), bytes([3, 6, 0, 4, 0]))

    def test_binary_trivial_input_bridge_accepts_string_cnf_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cnf = root / 'input.cnf'
            lsr = root / 'proof.binary.lsr'
            cnf.write_text('p cnf 2 2\n1 2 0\n0\n')
            lsr.write_bytes(bytes([3, 6, 0, 0]))
            self.assertTrue(bridge_trivial_input_binary_lsr(str(cnf), lsr))
            self.assertEqual(lsr.read_bytes(), bytes([3, 6, 0, 4, 0]))

    def test_binary_trivial_input_bridge_accepts_anonymous_fd_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cnf = root / 'input.cnf'
            cnf.write_text('p cnf 2 2\n1 2 0\n0\n')
            fd = os.open(root, os.O_TMPFILE | os.O_RDWR | os.O_CLOEXEC, 0o600)
            try:
                lsr = Path(f'/proc/{os.getpid()}/fd/{fd}')
                lsr.write_bytes(bytes([3, 6, 0, 0]))
                self.assertTrue(bridge_trivial_input_binary_lsr(cnf, lsr))
                self.assertEqual(lsr.read_bytes(), bytes([3, 6, 0, 4, 0]))
            finally:
                os.close(fd)

    def test_binary_bridge_is_narrow(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cnf = root / 'input.cnf'
            cnf.write_text('p cnf 1 1\n1 0\n')
            for payload in (bytes([3, 4, 0, 0]), bytes([3, 4, 0, 128])):
                lsr = root / 'proof.binary.lsr'
                lsr.write_bytes(payload)
                self.assertFalse(bridge_trivial_input_binary_lsr(cnf, lsr))
                self.assertEqual(lsr.read_bytes(), payload)

if __name__ == '__main__':
    unittest.main()
