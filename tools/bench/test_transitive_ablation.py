import unittest

from transitive_ablation import validate_activation


class TransitiveAblationTests(unittest.TestCase):
    timers = ['c timing 10 ms solve',
              'c timing 4 ms (40.0%) inprocessing slot, 6 ms (60.0%) search+scaffolding outside it']
    zero = 'c transitive 0 rounds, 0 checked, 0 deleted, 0 units, 0 ticks, 0 polls, 0 terminated'
    active = 'c transitive 0 rounds, 2 checked, 1 deleted, 0 units, 7 ticks, 2 polls, 0 terminated'

    def test_current_on_and_off(self):
        self.assertEqual(validate_activation([self.zero] + self.timers, 'candidate', False)['ticks'], 0)
        self.assertEqual(validate_activation([self.active] + self.timers, 'candidate', True)['ticks'], 7)

    def test_current_mismatched_work_or_application_rejected(self):
        for row, enabled, extra in ((self.active, False, []), (self.zero, True, []),
                (self.zero, False, ['c timing TransitiveReduce 0 ms (0.0%) in 1 applications'])):
            with self.assertRaises(ValueError):
                validate_activation([row] + self.timers + extra, 'candidate', enabled)

    def test_missing_duplicate_malformed_rows_rejected(self):
        for rows in ([], [self.zero, self.zero], [self.zero.replace('0 ticks', '-1 ticks')]):
            with self.assertRaises(ValueError):
                validate_activation(rows + self.timers, 'candidate', False)
        for rows in ([], ['c transitive_ticks: 0', 'c transitive_ticks: 0'], ['c transitive_ticks: -1']):
            with self.assertRaises(ValueError):
                validate_activation(rows, 'kissat', False)

    def test_native_zero_is_observed_not_assumed_from_omission(self):
        for ticks, enabled in ((0, False), (7, True)):
            rows = [f'c transitive_ticks: {ticks} 0 % ticks']
            self.assertEqual(validate_activation(rows, 'kissat', enabled), {'ticks': ticks})
            with self.assertRaises(ValueError):
                validate_activation(rows, 'kissat', not enabled)

    def test_unknown_solver_and_non_boolean_mode_rejected(self):
        for solver, enabled in (('unknown', False), ('kissat', 0), ('candidate', 'off')):
            with self.assertRaises(ValueError):
                validate_activation([self.zero] + self.timers, solver, enabled)
