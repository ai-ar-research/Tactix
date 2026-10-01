"""Prove forward DRAT audits preserve the extra every-lemma obligation."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from fixed_runtime import CANONICAL_CHECKERS, sha
from proof_oracle import every_lemma_drat_stages, proof_check_status

ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / 'benchmarks/runs/20260928-am549-no-mab-hybrid-wv100/pins/drat-trim'
# The first four clauses refute x1/x2 without root propagation. The last two
# prevent either literal of the unused (x3 or x4) addition from being a RAT pivot.
INPUT = 'p cnf 5 6\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n-3 5 0\n-4 5 0\n'


class EveryLemmaDratTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if sha(CHECKER) != CANONICAL_CHECKERS['drat-trim']:
            raise RuntimeError('forward audit checker pin changed')

    def audit(self, work, proof):
        verdicts = []
        for name, command in every_lemma_drat_stages(CHECKER, work/'input', proof):
            done = subprocess.run(list(map(str, command)), capture_output=True,
                                  text=True, timeout=10)
            verdicts.append(proof_check_status(name, done.returncode, done.stdout, done.stderr))
        return verdicts

    def test_valid_forward_refutation_is_verified(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work/'input').write_text(INPUT)
            (work/'proof').write_text('1 0\n0\n')
            self.assertEqual(self.audit(work, work/'proof'), ['verified'])

    def test_unused_invalid_lemma_is_rejected_despite_valid_final_refutation(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work/'input').write_text(INPUT)
            proof = work/'proof'
            proof.write_text('3 4 0\nd 3 4 0\n1 0\n0\n')
            backward = subprocess.run([str(CHECKER), str(work/'input'), str(proof)],
                capture_output=True, text=True, timeout=10)
            self.assertEqual(proof_check_status('drat-trim', backward.returncode,
                                              backward.stdout, backward.stderr), 'verified')
            self.assertEqual(self.audit(work, proof), ['rejected'])


if __name__ == '__main__':
    unittest.main()
