import unittest

from inprocessing_cost import read_timers


class InprocessingCostTests(unittest.TestCase):
    rows = ['c timing      1001 ms solve',
        'c timing      600 ms (60.0%) inprocessing slot, 400 ms (40.0%) search+scaffolding outside it',
        'c timing        Vivify               500 ms ( 50.0%) in 4 applications',
        'c timing        (unattributed)        100 ms ( 10.0%)']

    def test_exact_fields_not_rounded_percentages(self):
        r=read_timers(self.rows)
        self.assertEqual(r['solve_ms'],1001)
        self.assertEqual(r['slot_ms'],600)
        self.assertEqual(r['rules']['Vivify'],dict(ms=500,applications=4))
        self.assertEqual(r['unattributed_ms'],100)

    def test_missing_and_duplicate_rows(self):
        for rows in ([],self.rows[1:],self.rows[:1]+self.rows[2:]):
            with self.assertRaises(ValueError):read_timers(rows)
        for row in self.rows:
            with self.assertRaises(ValueError):read_timers(self.rows+[row])

    def test_unknown_or_malformed_timing(self):
        for bad in ('c timing bogus','c timing -1 ms solve','c timing Vivify x ms (1.0%) in 2 applications'):
            with self.assertRaises(ValueError):read_timers(self.rows+[bad])

    def test_partition_rounding(self):
        for value in ('999','1000','1001'):
            self.assertEqual(read_timers([self.rows[0].replace('1001',value)]+self.rows[1:])['solve_ms'],int(value))
        for value in ('998','1002'):
            with self.assertRaises(ValueError):read_timers([self.rows[0].replace('1001',value)]+self.rows[1:])

    def test_zero_slot_and_non_timer_rows(self):
        r=read_timers(['s UNSATISFIABLE','c conflicts 10','c timing 1 ms solve',
            'c timing 0 ms (0.0%) inprocessing slot, 1 ms (100.0%) search+scaffolding outside it'])
        self.assertEqual(r['rules'],{})
        self.assertEqual(r['slot_ms'],0)


if __name__=='__main__':unittest.main()
