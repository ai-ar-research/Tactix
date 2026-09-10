import unittest
import copy

from inprocessing_cost import read_timers, paired_costs


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


class PairedCostTests(unittest.TestCase):
    def rows(self):
        rows = []
        for wave in range(3):
            for rank in (32,39):
                for arm in (('previous','candidate') if wave == 1 else ('candidate','previous')):
                    rows.append(dict(task=len(rows),rank=rank,arm=arm,
                        solver_statistics=list(InprocessingCostTests.rows),
                        solve=dict(wall_seconds=10+wave+(arm == 'candidate'),peak_rss_kb=100)))
        return rows

    def summarize(self, rows):
        return paired_costs(rows,(32,39),3,('Vivify',))

    def test_all_waves_and_reversed_middle_order(self):
        rows = self.rows(); original = copy.deepcopy(rows)
        result = self.summarize(list(reversed(rows)))
        self.assertEqual(rows,original)
        self.assertEqual(result[32]['paired']['wall_seconds'],dict(
            priced_waves=3, median=12/11, min=13/12, max=11/10))
        self.assertEqual(result[39]['paired']['Vivify_ms']['median'],1)
        self.assertEqual(result[39]['waves'][1]['applications'],{'Vivify':4})

    def test_missing_duplicate_and_out_of_range_task(self):
        rows = self.rows()
        for bad in (rows[:-1],rows+[rows[0]]):
            with self.assertRaises(ValueError): self.summarize(bad)
        for task in (-1,12,True,1.5):
            bad=copy.deepcopy(rows);bad[0]['task']=task
            with self.assertRaises(ValueError): self.summarize(bad)

    def test_wrong_arm_rank_or_pair(self):
        for key,value in (('arm','unknown'),('rank',40),('arm','previous'),('rank',39)):
            rows=self.rows();rows[0][key]=value
            with self.assertRaises(ValueError): self.summarize(rows)

    def test_changed_application_counts(self):
        rows=self.rows()
        rows[0]['solver_statistics'][2]=rows[0]['solver_statistics'][2].replace('in 4','in 5')
        with self.assertRaisesRegex(ValueError,'application'): self.summarize(rows)

    def test_missing_rule(self):
        with self.assertRaisesRegex(ValueError,'missing requested'):
            paired_costs(self.rows(),(32,39),3,('Sweep',))

    def test_zero_denominator_does_not_select_remaining_waves(self):
        rows=self.rows()
        rows[1]['solver_statistics'][2]=rows[1]['solver_statistics'][2].replace('500 ms','0 ms')
        result=self.summarize(rows)[32]
        self.assertIsNone(result['waves'][0]['ratios']['Vivify_ms'])
        self.assertEqual(result['paired']['Vivify_ms'],dict(priced_waves=2,median=None,min=None,max=None))

    def test_invalid_measurements(self):
        for key,value in (('wall_seconds',0),('wall_seconds',float('nan')),
                          ('wall_seconds',float('inf')),('wall_seconds',True),
                          ('peak_rss_kb',0),('peak_rss_kb',1.5),('peak_rss_kb',True)):
            rows=self.rows();rows[0]['solve'][key]=value
            with self.assertRaisesRegex(ValueError,'invalid wall'): self.summarize(rows)

    def test_invalid_declarations(self):
        for ranks,waves,rules in (((),3,('Vivify',)),((32,32),3,('Vivify',)),
                                 ((32,39),0,('Vivify',)),((32,39),True,('Vivify',)),
                                 ((32,39),3,()),((32,39),3,('Vivify','Vivify'))):
            with self.assertRaises(ValueError): paired_costs(self.rows(),ranks,waves,rules)


if __name__=='__main__':unittest.main()
