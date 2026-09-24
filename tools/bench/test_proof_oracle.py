from pathlib import Path
import os
import tempfile
import unittest

from proof_oracle import bridge_trivial_input_binary_lsr, proof_check_status


class ProofOracleTests(unittest.TestCase):
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
