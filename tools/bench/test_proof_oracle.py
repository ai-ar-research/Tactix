from pathlib import Path
import tempfile
import unittest

from proof_oracle import bridge_trivial_input_binary_lsr, proof_check_status


class ProofOracleTests(unittest.TestCase):
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
