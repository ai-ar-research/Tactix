import unittest

from proof_oracle import proof_check_status


class ProofOracleTests(unittest.TestCase):
    def test_exact_drat_success_with_carriage_return(self):
        self.assertEqual(proof_check_status('drat-trim', 0, '\rs VERIFIED\n', ''), 'verified')

    def test_exact_cake_success(self):
        self.assertEqual(proof_check_status('cake_lpr', 0, 's VERIFIED UNSAT\n', ''), 'verified')

    def test_rejection_precedes_success(self):
        self.assertEqual(proof_check_status('drat-trim', 1, 's VERIFIED\ns NOT VERIFIED\n', ''), 'rejected')

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
        for code in (1, -9):
            self.assertEqual(proof_check_status('cake_lpr', code, 's VERIFIED UNSAT\n', ''), 'error')

    def test_legacy_checker_cannot_be_selected(self):
        with self.assertRaises(ValueError):
            proof_check_status('lrat-check', 0, 'c VERIFIED\n', '')


if __name__ == '__main__':
    unittest.main()
