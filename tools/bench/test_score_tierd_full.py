#!/usr/bin/env python3
"""Decision boundaries for the full Tier D evidence scorer."""
import copy
import unittest

from score_tierd_full import classify


class TierDClassification(unittest.TestCase):
    def setUp(self):
        self.row = dict(task=0, rank=1, arm='satsuma-kissat',
                        instance='example.cnf.xz', expected='UNKNOWN')
        self.unsat = dict(self.row, verdict='UNSAT', expected_match=True,
                          artifact_deleted=True, derived_artifacts_deleted=True,
                          cleanup_pending=False, valid=True,
                          solve=dict(wall_seconds=3.0, exit_code=20,
                                     timed_out=False),
                          proof_check=dict(result='verified', timed_out=False,
                                           stages=[dict(name='drat-trim', result='verified'),
                                                   dict(name='cake-lpr', result='verified')]),
                          model_check=None)

    def test_unknown_reference_label_accepts_independently_checked_unsat(self):
        self.assertEqual(classify(self.row, self.unsat, 'satsuma-kissat')[0],
                         'checked_UNSAT')

    def test_checker_timeout_is_not_a_verified_solve(self):
        result = copy.deepcopy(self.unsat)
        result['valid'] = False
        result['proof_check']['result'] = 'not-verified'
        result['proof_check']['timed_out'] = True
        self.assertEqual(classify(self.row, result, 'satsuma-kissat')[0],
                         'checker_timeout')

    def test_wrong_route_and_false_proof_are_rejected(self):
        wrong = copy.deepcopy(self.unsat)
        wrong['proof_check']['stages'][1]['name'] = 'srcheck'
        self.assertEqual(classify(self.row, wrong, 'satsuma-kissat')[0],
                         'proof_route_or_stage_failure')
        false = copy.deepcopy(self.unsat)
        false['valid'] = False
        false['proof_check']['result'] = 'not-verified'
        self.assertEqual(classify(self.row, false, 'satsuma-kissat')[0],
                         'certificate_rejected')

    def test_solver_timeout_is_not_an_unsat_certificate(self):
        result = copy.deepcopy(self.unsat)
        result.update(verdict='UNKNOWN', valid=False, proof_check=None)
        result['solve'].update(exit_code=None, timed_out=True,
                               wall_seconds=5000.0)
        self.assertEqual(classify(self.row, result, 'satsuma-kissat')[0],
                         'solver_timeout')

    def test_scratch_resource_incomplete_is_never_scored_as_a_verdict(self):
        result = copy.deepcopy(self.unsat)
        result.update(valid=False, verdict='INFRA_ERROR', solve=None,
                      completion=dict(status='incomplete',
                                      reason='scratch_file_limit'))
        status, seconds, reason = classify(
            self.row, result, 'satsuma-kissat')
        self.assertEqual((status, seconds, reason),
                         ('resource_incomplete', None, 'scratch_file_limit'))
        result['completion']['reason'] = 'arbitrary_incomplete'
        with self.assertRaisesRegex(ValueError,
                                    'unsupported incomplete resource reason'):
            classify(self.row, result, 'satsuma-kissat')


if __name__ == '__main__':
    unittest.main()
