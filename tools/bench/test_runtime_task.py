"""Non-solving acceptance tests: UNKNOWN is a dataset label, not an oracle."""
import json
from pathlib import Path
import tempfile
import unittest
from runtime_task import (atomic_json, checked_verdict_valid, expected_matches,
                          positive_timeout, provisional_result, solver_command)


class RuntimeTaskTests(unittest.TestCase):
    def test_atomic_json_creates_missing_output_directory(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "results" / "007.json"
            atomic_json(path, {"task": 7})
            self.assertEqual(json.loads(path.read_text()), {"task": 7})
            self.assertFalse(path.with_suffix(".json.tmp").exists())

    def sample_result(self):
        return dict(task=0, rank=32, arm='tactix', tier='D', instance='fixed.cnf',
                    expected='UNSAT', verdict='UNSAT', expected_match=True,
                    solve=dict(exit_code=20, timed_out=False), node='test',
                    certificate_bytes=12, certificate_sha256='hash',
                    solver_statistics=['c conflicts 1'],
                    solver_stderr_tail=['[cong-time] stage output'],
                    valid=False, cleanup_pending=True, proof_check=None)

    def test_provisional_publishes_diagnostics_without_changing_work(self):
        original = self.sample_result()
        projected = provisional_result(original, 'fixed timestamp')
        for key in ('solver_stderr_tail', 'solver_statistics', 'solve',
                    'certificate_bytes', 'certificate_sha256', 'expected_match'):
            self.assertEqual(projected[key], original[key])
        self.assertEqual(projected['recorded_at'], 'fixed timestamp')
        self.assertTrue(projected['validation_pending'])
        self.assertNotIn('validation_pending', original)

    def test_provisional_never_inherits_final_acceptance(self):
        original = self.sample_result()
        original.update(valid=True, proof_check={'result': 'verified'}, model_check={'valid': True},
                        artifact_deleted=True, cleanup_pending=False)
        original['solver_stderr_tail'] = ['s VERIFIED']
        projected = provisional_result(original, 'fixed')
        self.assertTrue(projected['validation_pending'])
        for key in ('valid', 'proof_check', 'model_check', 'artifact_deleted', 'cleanup_pending'):
            self.assertNotIn(key, projected)

    def test_empty_diagnostics_and_nonmatching_verdict_are_preserved(self):
        original = self.sample_result()
        original.update(solver_stderr_tail=[], verdict='UNKNOWN', expected_match=False)
        projected = provisional_result(original, 'fixed')
        self.assertEqual(projected['solver_stderr_tail'], [])
        self.assertEqual(projected['verdict'], 'UNKNOWN')
        self.assertFalse(projected['expected_match'])

    def test_known_label_requires_matching_verdict(self):
        for expected in ('SAT', 'UNSAT'):
            for verdict in ('SAT', 'UNSAT', 'UNKNOWN', 'INFRA_ERROR'):
                self.assertEqual(expected_matches(expected, verdict), expected == verdict)

    def test_unknown_label_admits_only_checked_solver_verdicts(self):
        for verdict, code in [('SAT', 10), ('UNSAT', 20)]:
            solve = dict(exit_code=code, timed_out=False)
            self.assertTrue(checked_verdict_valid('UNKNOWN', verdict, solve, True))
            self.assertFalse(checked_verdict_valid('UNKNOWN', verdict, solve, False))

    def test_timeout_error_wrong_exit_or_known_disagreement_is_never_valid(self):
        for expected in ('SAT', 'UNSAT', 'UNKNOWN'):
            for verdict in ('SAT', 'UNSAT', 'UNKNOWN', 'INFRA_ERROR'):
                for code in (None, 0, 1, 10, 20, -9, 124):
                    for timeout in (False, True):
                        for checked in (False, True):
                            want = (checked and not timeout and verdict in ('SAT', 'UNSAT')
                                    and expected in ('UNKNOWN', verdict)
                                    and code == {'SAT': 10, 'UNSAT': 20}.get(verdict))
                            self.assertEqual(checked_verdict_valid(expected, verdict,
                                dict(exit_code=code, timed_out=timeout), checked), want)

    def test_manifest_timeouts_are_enforced(self):
        manifest = {'resource_contract': {'solve_timeout': 300,
                                          'checker_timeout': 16000}}
        self.assertEqual(positive_timeout(manifest, 'solve_timeout'), 300)
        self.assertEqual(positive_timeout(manifest, 'checker_timeout'), 16000)
        for value in (None, True, False, 0, -1, 1.0, '300'):
            manifest['resource_contract']['solve_timeout'] = value
            with self.assertRaises(RuntimeError):
                positive_timeout(manifest, 'solve_timeout')

    def test_solver_command_supports_validated_trailing_arguments(self):
        arm = {'arguments': ['--before'],
               'trailing_arguments': ['dsr', 'SymmetrySR']}
        self.assertEqual(
            solver_command('solver', arm, 'input.cnf', 'proof.dsr'),
            ['solver', '--before', 'input.cnf', 'proof.dsr', 'dsr',
             'SymmetrySR'])
        for arm in ({'arguments': 'bad'},
                    {'trailing_arguments': [1]},
                    {'arguments': [], 'trailing_arguments': 'bad'}):
            with self.assertRaises(RuntimeError):
                solver_command('solver', arm, 'input.cnf', 'proof.dsr')


if __name__ == '__main__':
    unittest.main()
