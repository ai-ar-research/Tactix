import copy
import unittest

from portfolio_targets import baseline_eligible, genuine_timeout, select


class PortfolioTargetsTests(unittest.TestCase):
    def baseline(self, verdict='SAT'):
        return dict(verdict=verdict, expected=verdict, expected_match=True,
            solver='lymphosat' if verdict == 'SAT' else 'kissat-4.0.4',
            vbs_eligible='SAT-only' if verdict == 'SAT' else 'SAT+UNSAT',
            valid=True, artifact_deleted=True, derived_artifacts_deleted=True,
            cleanup_pending=False, model_ok=True, proof_ok=True,
            solve=dict(exit_code=10 if verdict == 'SAT' else 20, timed_out=False,
                       wall_seconds=1200, peak_rss_kb=32*1024*1024),
            model_check=dict(exit_code=0, result='verified', timed_out=False),
            proof_check=dict(result='verified', stages=[dict(exit_code=0,
                             result='verified', timed_out=False)]))

    def eligible(self, row):
        return baseline_eligible(row, 1200, 32*1024*1024)

    def test_budget_boundaries_and_invalid_metrics(self):
        row = self.baseline()
        self.assertTrue(self.eligible(row))
        for field, values in [('wall_seconds', [1200.001, -1, float('nan'), True, None]),
                              ('peak_rss_kb', [32*1024*1024+1, -1, float('inf'), None])]:
            for value in values:
                with self.subTest(field=field, value=value):
                    bad = copy.deepcopy(row); bad['solve'][field] = value
                    self.assertFalse(self.eligible(bad))

    def test_baseline_invalidity_is_not_a_solve(self):
        for field, value in [('valid', False), ('expected_match', False),
                             ('expected', 'UNSAT'), ('model_ok', False),
                             ('artifact_deleted', False), ('cleanup_pending', True)]:
            row = self.baseline(); row[field] = value
            self.assertFalse(self.eligible(row))
        for field, value in [('exit_code', -9), ('timed_out', True)]:
            row = self.baseline(); row['solve'][field] = value
            self.assertFalse(self.eligible(row))

    def test_unsat_requires_all_oracles_and_allows_checked_lymphosat(self):
        row = self.baseline('UNSAT'); self.assertTrue(self.eligible(row))
        for field, value in [('exit_code', 1), ('result', 'not-verified'), ('timed_out', True)]:
            bad = copy.deepcopy(row); bad['proof_check']['stages'][0][field] = value
            self.assertFalse(self.eligible(bad))
        row['solver'] = 'lymphosat'; row['vbs_eligible'] = 'SAT-only'
        self.assertTrue(self.eligible(row))

    def test_only_genuine_final_solver_timeout(self):
        row = self.baseline(); row.update(verdict='UNKNOWN', model_check=None, proof_check=None)
        row['solve'].update(timed_out=True, exit_code=-15)
        self.assertTrue(genuine_timeout(row, True, 1200, 32*1024*1024))
        self.assertFalse(genuine_timeout(row, False, 1200, 32*1024*1024))
        for field, value in [('timed_out', False), ('exit_code', 10), ('wall_seconds', 1199),
                             ('peak_rss_kb', 32*1024*1024+1)]:
            bad = copy.deepcopy(row); bad['solve'][field] = value
            self.assertFalse(genuine_timeout(bad, True, 1200, 32*1024*1024))
        row['model_check'] = {'result': 'not-verified'}
        self.assertFalse(genuine_timeout(row, True, 1200, 32*1024*1024))

    def test_timeout_without_time_peak_requires_matching_cgroup_contract(self):
        row = self.baseline(); row.update(verdict='UNKNOWN', model_check=None,
                                          proof_check=None,
                                          memory_enforcement='slurm_task_cgroup')
        row['solve'].update(timed_out=True, exit_code=None, wall_seconds=1200.004,
                            peak_rss_kb=None)
        self.assertFalse(genuine_timeout(row, True, 1200, 32*1024*1024))
        self.assertTrue(genuine_timeout(row, True, 1200, 32*1024*1024,
                                        matching_cgroup_contract=True))
        row['memory_enforcement'] = 'legacy RLIMIT_AS'
        self.assertFalse(genuine_timeout(row, True, 1200, 32*1024*1024,
                                         matching_cgroup_contract=True))
        row['memory_enforcement'] = 'slurm_task_cgroup'
        row['solve']['peak_rss_kb'] = 32*1024*1024 + 1
        self.assertFalse(genuine_timeout(row, True, 1200, 32*1024*1024,
                                         matching_cgroup_contract=True))

    def test_rarity_lymphosat_preference_fastest_and_quotas(self):
        def candidate(name, solvers, seconds, verdict='SAT'):
            return dict(instance=name, solvers=sorted(solvers), fastest_seconds=seconds, verdict=verdict)
        rows = [candidate('shared', ['lymphosat', 'kissat'], .1),
                candidate('other-only', ['kissat'], 1),
                candidate('lympho-only-slower', ['lymphosat'], 100),
                candidate('lympho-only-faster', ['lymphosat'], 50),
                candidate('unsat', ['kissat'], 2, 'UNSAT')]
        selected = select(rows, 2)
        self.assertEqual([r['instance'] for r in selected],
                         ['lympho-only-faster', 'lympho-only-slower', 'unsat'])
        self.assertEqual(len(select(rows, 10)), len(rows))


if __name__ == '__main__':
    unittest.main()
