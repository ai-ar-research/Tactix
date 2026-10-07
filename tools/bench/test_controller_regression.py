"""Acceptance boundaries for the user-defined bounded regression protocol."""
import unittest
import json
from pathlib import Path
import tempfile
from unittest.mock import patch
import controller_regression as regression
from controller_regression import proof_disposition, proof_cost_comparison, score_row, candidate_arguments


def proof(elapsed, timeout=False):
    return dict(pipeline='dsr-as-drat-drat-trim-cake_lpr',
        result='not-verified' if timeout else 'verified', timed_out=timeout,
        wall_seconds=elapsed, stages=[dict(name='drat-trim', result='verified'),
            dict(name='pipeline-deadline' if timeout else 'cake_lpr',
                 result='timeout' if timeout else 'verified', timed_out=timeout)])


class CandidateArgumentTests(unittest.TestCase):
    def test_legacy_command_unchanged_and_profile_exact(self):
        self.assertEqual(candidate_arguments(), ['--auto-proof'])
        for mode in ('on', 'off'):
            argument = '--default-affine-search=' + mode
            self.assertEqual(candidate_arguments([argument]), ['--auto-proof', argument])
            argument = '--default-nol-walk=' + mode
            self.assertEqual(candidate_arguments([argument]), ['--auto-proof', argument])
        self.assertEqual(
            candidate_arguments(['--default-affine-search=off', '--default-nol-walk=on']),
            ['--auto-proof', '--default-affine-search=off', '--default-nol-walk=on'])

    def test_rejects_empty_proof_policy_and_conflicting_overrides(self):
        for arguments in ([''], ['--drat'], ['--auto-proof'], ['--lrat'],
                          ['--random-policy'], ['--default-affine-search=maybe'],
                          ['--default-nol-walk=maybe'],
                          [' --default-affine-search=on'],
                          ['--default-affine-search=on --drat'],
                          ['--default-affine-search=on', '--default-affine-search=off'],
                          ['--default-affine-search=on', '--default-affine-search=on']):
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                candidate_arguments(arguments)
        # Reject before any authority/pin read or campaign mutation.
        with self.assertRaises(ValueError):
            regression.prepare('invalid', Path('/nonexistent'), ['--lrat'])

    def test_preparation_binds_exact_arguments_without_changing_baseline(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            historical = root / 'benchmarks/runs/historical'
            pins = historical / 'pins'
            pins.mkdir(parents=True)
            tools = root / 'tools/bench'; tools.mkdir(parents=True)
            for name in ('runtime_task.py', 'proof_oracle.py', 'anonymous_scratch.py'):
                (tools / name).write_text('# fixed non-executed protocol fixture\n')
            checkers = {'checker': None}
            for name in ('checkmodel', 'modelcheck.py', *checkers):
                (pins / name).write_text('fixed protocol artifact ' + name)
            checkers['checker'] = regression.sha(pins / 'checker')
            old = {'arms': {'tactix': {'arguments': ['--auto-proof']}}}
            source = historical / 'manifest.json'; source.write_text(json.dumps(old))
            matrix = historical / 'matrix.tsv'; matrix.write_text('instance\nfixture.cnf\n')
            cache = root / 'benchmarks/cache'; cache.mkdir()
            (cache / 'fixture.cnf').write_text('p cnf 1 1\n1 0\n')
            accepted = dict(source_manifest=str(source.relative_to(root)),
                source_manifest_sha256=regression.sha(source),
                input_sha256={'fixture.cnf': regression.sha(cache / 'fixture.cnf')},
                pin_sha256={'pins/' + name: regression.sha(pins / name)
                            for name in ('checkmodel', 'modelcheck.py', *checkers)},
                matrix_sha256=regression.sha(matrix), expected_rows=1,
                resource_contract=dict(partition='fixture', cpus_per_task=2,
                                       memory_mib=8192, solve_timeout=10,
                                       one_solver_per_task=True))
            baseline = root / 'baseline.json'; baseline.write_text(json.dumps({'sets': {'wv100': accepted}}))
            release = root / 'build/pin-fixture/tactix'; release.parent.mkdir(parents=True)
            release.write_text('fixed non-executed solver fixture')
            (release.parent / 'PIN.json').write_text(json.dumps(dict(build_type='Release',
                source_commit='fixed-fixture', binaries={'tactix': regression.sha(release)})))
            baseline_before = baseline.read_bytes()
            with patch.object(regression, 'ROOT', root), patch.object(regression, 'BASELINE', baseline), \
                 patch.object(regression, 'CANONICAL_CHECKERS', checkers), \
                 patch.object(regression.subprocess, 'run') as verified:
                for prefix, arguments in (('legacy', []), ('enabled', ['--default-affine-search=on'])):
                    campaigns = regression.prepare(prefix, release, arguments)
                    manifest = json.loads((Path(campaigns[0]) / 'manifest.json').read_text())
                    self.assertEqual(manifest['arms']['tactix']['arguments'],
                                     ['--auto-proof', *arguments])
                    self.assertEqual(manifest['resource_contract']['throttle'], None)
                    self.assertEqual(manifest['baseline']['sha256'], regression.sha(baseline))
                self.assertEqual(verified.call_count, 2)
                for call in verified.call_args_list:
                    self.assertEqual(call.args[0][-1], '--verify')
            self.assertEqual(baseline.read_bytes(), baseline_before)
            self.assertEqual(json.loads(source.read_text()), old)


class RegressionGateTests(unittest.TestCase):
    def test_small_baseline_uses_twenty_seconds_and_equality_passes(self):
        self.assertEqual(proof_disposition(proof(30), proof(10)), ('verified', []))
        self.assertIn('significant', proof_disposition(proof(30.001), proof(10))[1][0])

    def test_large_baseline_uses_thirty_percent_and_equality_passes(self):
        self.assertEqual(proof_disposition(proof(130), proof(100)), ('verified', []))
        self.assertTrue(proof_disposition(proof(130.001), proof(100))[1])

    def test_timeout_within_cost_bound_remains_censored_and_can_pass(self):
        self.assertEqual(proof_disposition(proof(1800, True), proof(1500)), ('censored', []))

    def test_timeout_already_exceeding_completed_baseline_limit_fails(self):
        disposition, errors = proof_disposition(proof(1800, True), proof(100))
        self.assertEqual(disposition, 'censored')
        self.assertIn('significant proof-check cost regression', errors)

    def test_censored_baseline_supplies_no_completion_cost_claim(self):
        self.assertEqual(proof_disposition(proof(1800, True), proof(57600, True)), ('censored', []))

    def test_censored_baseline_cost_comparison_is_explicitly_inconclusive(self):
        for candidate in (proof(30), proof(1800, True)):
            comparison = proof_cost_comparison(candidate, proof(57600, True))
            self.assertEqual(comparison['status'], 'inconclusive')
            self.assertIsNone(comparison['allowed_completion_seconds'])
            self.assertTrue(comparison['baseline_censored'])

    def test_cost_dispositions_distinguish_completion_censoring_and_slowdown(self):
        for candidate, baseline, expected in (
                (proof(130), proof(100), 'completed-within-bound'),
                (proof(1800, True), proof(1500), 'censored-within-bound'),
                (proof(1800, True), proof(100), 'significant-slowdown')):
            comparison = proof_cost_comparison(candidate, baseline)
            self.assertEqual(comparison['status'], expected)
            self.assertEqual(comparison['candidate_censored'], candidate['timed_out'])

    def test_missing_baseline_is_inconclusive_even_for_fast_completed_check(self):
        comparison = proof_cost_comparison(proof(1), None)
        self.assertEqual(comparison['status'], 'inconclusive')
        self.assertIsNone(comparison['allowed_completion_seconds'])

    def test_cost_disposition_does_not_compare_checker_failure_as_fast_completion(self):
        failed = proof(1)
        failed['result'] = 'not-verified'
        self.assertEqual(proof_cost_comparison(failed, proof(100))['status'], 'checker-failure')
        failed = proof(1800, True)
        failed['stages'][0]['result'] = 'rejected'
        self.assertEqual(proof_cost_comparison(failed, proof(1500))['status'], 'checker-failure')

    def test_recorded_rejection_dominates_later_deadline(self):
        rejected = proof(1800, True)
        rejected['stages'][0]['result'] = 'rejected'
        self.assertEqual(proof_disposition(rejected, None)[0], 'failed')

    def test_missing_final_independent_verifier_is_not_verified(self):
        incomplete = proof(10)
        incomplete['stages'].pop()
        self.assertEqual(proof_disposition(incomplete, None)[0], 'failed')

    def test_bad_time_or_missing_outcome_fails(self):
        for elapsed in (-1, float('nan'), float('inf'), None):
            self.assertEqual(proof_disposition(proof(elapsed), None)[0], 'failed')
        self.assertEqual(proof_disposition(None, None)[0], 'missing')

    def sample(self):
        row = dict(artifact_deleted=True, derived_artifacts_deleted=True,
            cleanup_pending=False, verdict='UNSAT', expected_match=True,
            solve=dict(exit_code=20, timed_out=False, wall_seconds=1, peak_rss_kb=100),
            proof_check=proof(1800, True), valid=False)
        accepted = dict(expected='UNSAT', require_solver_retention=True, proof_check=proof(1500))
        contract = dict(solve_timeout=1200, memory_mib=32768)
        return row, accepted, contract

    def test_censored_result_stays_unverified_while_retention_gate_passes(self):
        row, accepted, contract = self.sample()
        self.assertEqual(score_row(row, accepted, contract), ('censored', []))
        self.assertFalse(row['valid'])

    def test_saved_solver_timeout_exception_does_not_excuse_checker_error(self):
        row, accepted, contract = self.sample()
        accepted['require_solver_retention'] = False
        row['solve']['timed_out'] = True
        row['proof_check']['stages'][0]['result'] = 'rejected'
        self.assertTrue(score_row(row, accepted, contract)[1])
        row['proof_check'] = None
        self.assertEqual(score_row(row, accepted, contract), ('solver-timeout', []))
        accepted['require_solver_retention'] = True
        self.assertIn('lost retained solve', score_row(row, accepted, contract)[1])

    def test_model_requires_independent_success(self):
        row, accepted, contract = self.sample()
        row.update(verdict='SAT', model_check=dict(result='verified', exit_code=0, timed_out=False))
        row['solve']['exit_code'] = 10
        accepted['expected'] = 'SAT'
        self.assertEqual(score_row(row, accepted, contract), ('model-verified', []))
        row['model_check']['exit_code'] = 1
        self.assertTrue(score_row(row, accepted, contract)[1])

    def test_cleanup_memory_solve_time_and_exit_failures(self):
        for key, value in [('wall_seconds', 1201), ('peak_rss_kb', 32768 * 1024 + 1),
                           ('exit_code', 0)]:
            row, accepted, contract = self.sample()
            row['solve'][key] = value
            self.assertTrue(score_row(row, accepted, contract)[1])
        row, accepted, contract = self.sample()
        row['cleanup_pending'] = True
        self.assertTrue(score_row(row, accepted, contract)[1])


if __name__ == '__main__':
    unittest.main()
