"""Tiny checker-only regressions; never run a solver or access fuzz artifacts."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from proof_oracle import proof_check_status

ROOT = Path(__file__).resolve().parents[2]


class ProofOracleBinaryTests(unittest.TestCase):
    def check_fixture(self, cnf, drat, lrat, expected):
        checkers = ROOT / 'tools/checkers/bin'
        for name in ('drat-trim', 'cake_lpr'):
            if not (checkers / name).is_file():
                self.skipTest('pinned checker unavailable: ' + name)
        with tempfile.TemporaryDirectory(prefix='proof-oracle-test-',
                                         dir=ROOT / 'benchmarks/probes/scratch') as directory:
            path = Path(directory)
            formula = path / 'input.cnf'
            formula.write_text(cnf)
            for name, text in (('drat-trim', drat), ('cake_lpr', lrat)):
                with self.subTest(checker=name):
                    proof = path / (name + '.proof')
                    proof.write_text(text)
                    result = subprocess.run([str(checkers / name), str(formula), str(proof)],
                                            capture_output=True, text=True, timeout=30)
                    self.assertEqual(proof_check_status(name, result.returncode,
                                                        result.stdout, result.stderr),
                                     expected, result.stdout + result.stderr)

    def test_satisfiable_unit_rejects_empty_lemma(self):
        # Reproducer6906: the legacy lrat-check accepted this non-conflicting
        # one-hint chain. Neither supported acceptance oracle may accept it.
        self.check_fixture('p cnf 1 1\n1 0\n', '0\n', '2 0 1 0\n', 'rejected')

    def test_actual_conflict_accepts_empty_lemma(self):
        self.check_fixture('p cnf 1 2\n1 0\n-1 0\n', '0\n',
                           '3 0 1 2 0\n', 'verified')


if __name__ == '__main__':
    unittest.main()
