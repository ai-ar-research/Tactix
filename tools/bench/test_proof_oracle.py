import unittest
from pathlib import Path
import tempfile

from proof_oracle import bridge_trivial_input_lsr, proof_check_status


class ProofOracleTests(unittest.TestCase):
    def test_exact_drat_success_with_carriage_return(self):
        self.assertEqual(proof_check_status('drat-trim', 0, '\rs VERIFIED\n', ''), 'verified')

    def test_exact_cake_success(self):
        self.assertEqual(proof_check_status('cake_lpr', 0, 's VERIFIED UNSAT\n', ''), 'verified')

    def test_exact_dsr_pipeline_successes(self):
        for checker in ('dsr-trim', 'srcheck'):
            with self.subTest(checker=checker):
                self.assertEqual(
                    proof_check_status(checker, 0, 's VERIFIED UNSAT\n', ''),
                    'verified')

    def test_dsr_and_trestle_rejections(self):
        self.assertEqual(
            proof_check_status('dsr-trim', 1, '', 'Error: invalid witness\n'),
            'rejected')
        # The pinned Trestle executable reports semantic invalidity with exit 0.
        self.assertEqual(
            proof_check_status('srcheck', 0, 's INVALID\n', ''), 'rejected')

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
        for checker in ('cake_lpr', 'dsr-trim', 'srcheck'):
            for code in (1, -9):
                with self.subTest(checker=checker, code=code):
                    self.assertEqual(
                        proof_check_status(checker, code,
                                           's VERIFIED UNSAT\n', ''), 'error')

    def test_legacy_checker_cannot_be_selected(self):
        with self.assertRaises(ValueError):
            proof_check_status('lrat-check', 0, 'c VERIFIED\n', '')

    def test_trivial_input_lsr_gets_exact_empty_clause_hint(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cnf, lsr = root / 'input.cnf', root / 'proof.lsr'
            cnf.write_text('c split clauses are legal\np cnf 2 2\n1\n2 0\n0\n')
            lsr.write_text('3 0 0\n')
            self.assertTrue(bridge_trivial_input_lsr(cnf, lsr))
            self.assertEqual(lsr.read_text(), '3 0 2 0\n')

    def test_trivial_bridge_refuses_nonempty_or_noncanonical_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cnf, lsr = root / 'input.cnf', root / 'proof.lsr'
            for formula, proof in (
                    ('p cnf 1 1\n1 0\n', '2 0 0\n'),
                    ('p cnf 1 1\n0\n', '2 0 1 0\n'),
                    ('p cnf 1 1\n0\n', '9 0 0\n')):
                cnf.write_text(formula)
                lsr.write_text(proof)
                self.assertFalse(bridge_trivial_input_lsr(cnf, lsr))
                self.assertEqual(lsr.read_text(), proof)

    def test_trivial_bridge_streams_past_a_large_real_proof_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cnf, lsr = root / 'input.cnf', root / 'proof.lsr'
            cnf.write_text('p cnf 1 1\n0\n')
            proof = b'2 0 1 0 ' + b'7 ' * 1_000_000
            lsr.write_bytes(proof)
            self.assertFalse(bridge_trivial_input_lsr(cnf, lsr))
            self.assertEqual(lsr.stat().st_size, len(proof))


if __name__ == '__main__':
    unittest.main()
