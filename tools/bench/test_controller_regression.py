"""Acceptance boundaries for the user-defined bounded regression protocol."""
import unittest
from controller_regression import proof_disposition, proof_cost_comparison, score_row


def proof(elapsed, timeout=False):
    return dict(pipeline='dsr-as-drat-drat-trim-cake_lpr',
        result='not-verified' if timeout else 'verified', timed_out=timeout,
        wall_seconds=elapsed, stages=[dict(name='drat-trim', result='verified'),
            dict(name='pipeline-deadline' if timeout else 'cake_lpr',
                 result='timeout' if timeout else 'verified', timed_out=timeout)])


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
