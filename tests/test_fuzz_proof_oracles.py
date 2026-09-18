#!/usr/bin/env python3
"""Exercise the actual fuzz dispatch with fixed proofs and real checkers."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools/fuzz'))
import fuzz


class FuzzProofOracles(unittest.TestCase):
    def fixture(self, cnf_text, proof_text, lrat=True, checker_response=None,
                convert=False, dsr=False):
        with tempfile.TemporaryDirectory(prefix='fuzz-oracle-fixed-') as directory:
            work = Path(directory); cnf = work/'input.cnf'; cnf.write_text(cnf_text)
            actual = fuzz.run_cmd
            def run(command, timeout):
                if command[0] == 'fixed-proof-emitter':
                    Path(command[-1]).write_text(proof_text)
                    return 20, 's UNSATISFIABLE\n', ''
                if checker_response is not None: return checker_response
                return actual(command, timeout)
            solver = ['fixed-proof-emitter'] + (
                ['--dsr', '--no-binary'] if dsr else
                (['--lrat'] if lrat else ['--drat']))
            with patch.object(fuzz, 'run_cmd', side_effect=run):
                result = fuzz.run_case(str(cnf), solver, None, 10, convert, str(work))
            self.assertTrue(result.checker_runs)
            if result.failures:
                bundle = Path(fuzz.save_bundle(str(work/'failures'), 0, 0, 'fixed-checker-control',
                                              str(cnf), result, solver, None, 10, convert, False))
                proof_name = ('proof.dsr' if dsr else
                              ('proof.lrat' if lrat else 'proof.drat'))
                self.assertEqual((bundle/proof_name).read_text(), proof_text)
                self.assertEqual(json.loads((bundle/'meta.json').read_text())['checker_runs'], result.checker_runs)
            return result

    def test_native_nonconflicting_empty_lemma_rejected(self):
        r = self.fixture('p cnf 1 1\n1 0\n', '2 0 1 0\n')
        self.assertIn('proof', [kind for kind, _ in r.failures])
        self.assertEqual(Path(r.checker_runs[0]['command'][0]).name, 'cake_lpr')
        self.assertEqual(r.checker_runs[0]['status'], 'rejected')

    def test_native_actual_conflict_accepted(self):
        self.assertFalse(self.fixture('p cnf 1 2\n1 0\n-1 0\n', '3 0 1 2 0\n').failures)

    def test_drat_nonconflicting_empty_lemma_rejected(self):
        self.assertTrue(self.fixture('p cnf 1 1\n1 0\n', '0\n', lrat=False).failures)

    def test_drat_actual_conflict_accepted(self):
        self.assertFalse(self.fixture('p cnf 1 2\n1 0\n-1 0\n', '0\n', lrat=False).failures)

    def test_drat_empty_input_checker_exit_one(self):
        r = self.fixture('p cnf 1 1\n0\n', '', lrat=False)
        self.assertFalse(r.failures)
        self.assertEqual(r.checker_runs[0]['exit_code'], 1)

    def test_drat_trivial_text_requires_actual_empty_input(self):
        r = self.fixture('p cnf 1 1\n1 0\n', '0\n', lrat=False,
                         checker_response=(1, 'c trivial UNSAT\ns VERIFIED\n', ''))
        self.assertTrue(r.failures)

    def test_default_dsr_pipeline_accepts_actual_conflict(self):
        r = self.fixture('p cnf 1 2\n1 0\n-1 0\n', '0\n', dsr=True)
        self.assertFalse(r.failures)
        self.assertEqual([Path(run['command'][0]).name
                         for run in r.checker_runs],
                         ['dsr-trim', 'srcheck'])
        self.assertEqual(r.checker_runs[-1]['command'][-1], 'c')

    def test_default_dsr_rejects_nonconflicting_empty_lemma(self):
        r = self.fixture('p cnf 1 1\n1 0\n', '0\n', dsr=True)
        self.assertIn('proof', [kind for kind, _ in r.failures])
        self.assertEqual(Path(r.checker_runs[0]['command'][0]).name,
                         'dsr-trim')

    def test_conversion_dispatches_to_cake(self):
        r = self.fixture('p cnf 1 2\n1 0\n-1 0\n', '0\n', lrat=False, convert=True)
        self.assertFalse(r.failures)
        self.assertEqual([Path(r['command'][0]).name for r in r.checker_runs],
                         ['drat-trim','drat-trim','cake_lpr'])

    def test_no_false_success_from_status_or_text(self):
        for mode, token in ((True, 's VERIFIED UNSAT'), (False, 's VERIFIED')):
            for code, out, err in ((1, token, ''), (None, token, ''), (0, 'prefix '+token, ''),
                                   (0, token+'\ns NOT VERIFIED', ''), (0, '', ''),
                                   (0, token, 'Checking failed at 2')):
                with self.subTest(lrat=mode, code=code, stdout=out, stderr=err):
                    r = self.fixture('p cnf 1 1\n1 0\n', '2 0 1 0\n' if mode else '0\n',
                                     lrat=mode, checker_response=(code, out, err))
                    self.assertTrue(r.failures)

    def test_nonterminating_repro_uses_persistent_harness(self):
        with tempfile.TemporaryDirectory(prefix='fuzz-timeout-repro-') as directory:
            root = Path(directory)
            cnf = root/'input.cnf'; cnf.write_text('p cnf 1 0\n')
            outcome = fuzz.CaseOutcome(); outcome.verdict = 'TIMEOUT'
            replay = root/'harness'
            with patch.dict('os.environ', {
                    'TACTIX_FUZZ_REPLAY_HARNESS': str(replay),
                    'TACTIX_FUZZ_HARNESS_SHA256': 'abc123'}):
                bundle = Path(fuzz.save_nonterminating(
                    str(root/'out'), 4, 9, 'fixed-timeout', str(cnf), outcome,
                    ['solver', '--random-policy=9'], 5.0))
            self.assertIn(str(replay/'fuzz.py'), (bundle/'repro.sh').read_text())
            self.assertEqual(
                json.loads((bundle/'meta.json').read_text())['harness_sha256'],
                'abc123')


if __name__ == '__main__': unittest.main()
