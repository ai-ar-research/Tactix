import copy
import unittest
from proc_memory_analysis import smaps, summarize
from proc_memory_task import FIELDS


MAP = ('1000-3000 rw-p 00000000 00:00 0 [heap]\nSize: 8 kB\nRss: 4 kB\n'
       'Anonymous: 4 kB\nPrivate_Dirty: 4 kB\nSwap: 0 kB\nVmFlags: rd wr\n')


def fixture():
    kib = {k: 100 for k in FIELDS}
    raw = '\n'.join(f'{k}: {v} kB' for k, v in kib.items())
    row = dict(kind='sample', pid=1, starttime_ticks=7, before_ns=10, after_ns=11,
               kib=kib, status=raw)
    row['high_water_snapshot'] = dict(pid=1, starttime_ticks=7, before_ns=12,
        after_ns=13, smaps_before_ns=14, smaps_after_ns=15, smaps=MAP,
        status=raw, kib=kib.copy(), status_after_smaps=raw, kib_after_smaps=kib.copy())
    return [row, dict(kind='observer_stop', pid=1, starttime_ticks=7,
                     maximum_sampled_hwm_kib=100)]


class ProcMemoryAnalysisTest(unittest.TestCase):
    def test_maps_keep_raw_fields(self):
        region = smaps(MAP)[0]
        self.assertEqual(region['name'], '[heap]')
        self.assertEqual(region['kib']['Rss'], 4)
        self.assertEqual(region['fields']['VmFlags'], 'rd wr')

    def test_invalid_maps(self):
        for text in ('', MAP+MAP, MAP.replace('8 kB', '9 kB'),
                     MAP.replace('Rss: 4 kB', 'Rss: 12 kB'), MAP.replace('Swap: 0 kB', ''),
                     MAP.replace('Rss: 4 kB', 'Rss: 4 MB')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                smaps(text)

    def test_peak_is_not_smaps_rss(self):
        result = summarize(fixture())
        self.assertEqual(result['peak_hwm_kib'], 100)
        self.assertEqual(result['maximum_snapshot']['smaps_total_rss_kib'], 4)
        self.assertIsNone(result['previous_sample_after_ns'])

    def test_snapshot_race_explicit(self):
        rows = fixture()
        snapshot = rows[0]['high_water_snapshot']
        snapshot['kib_after_smaps']['VmHWM'] = 102
        snapshot['status_after_smaps'] = snapshot['status_after_smaps'].replace('VmHWM: 100', 'VmHWM: 102')
        self.assertFalse(summarize(rows)['maximum_snapshot']['hwm_unchanged_through_snapshot'])

    def test_snapshot_failure_preserved(self):
        rows = fixture()
        del rows[0]['high_water_snapshot']
        rows[0]['snapshot_error'] = 'process exited'
        result = summarize(rows)
        self.assertIsNone(result['maximum_snapshot'])
        self.assertEqual(result['snapshot_error'], 'process exited')

    def test_bad_records(self):
        mutations = [lambda r:r.pop(), lambda r:r[-1].update(pid=2),
            lambda r:r[-1].update(maximum_sampled_hwm_kib=101),
            lambda r:r[0].update(status=''), lambda r:r[0].update(after_ns=1),
            lambda r:r[0]['high_water_snapshot'].update(smaps_before_ns=0),
            lambda r:r[0].update(kind='unknown')]
        for mutate in mutations:
            rows = fixture()
            mutate(rows)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                summarize(rows)

    def test_gap_and_peak_bracket(self):
        rows = fixture()
        later = copy.deepcopy(rows[0])
        later.update(before_ns=20, after_ns=21)
        later['kib']['VmHWM'] = 110
        later['status'] = later['status'].replace('VmHWM: 100', 'VmHWM: 110')
        del later['high_water_snapshot']
        rows.insert(1, dict(kind='gap', error='explicit fixture gap'))
        rows.insert(2, later)
        rows[-1]['maximum_sampled_hwm_kib'] = 110
        result = summarize(rows)
        self.assertEqual(result['previous_sample_after_ns'], 11)
        self.assertEqual(result['first_maximum_row'], 2)
        self.assertEqual(len(result['gaps']), 1)

    def test_raw_high_water_decrease_is_not_silently_normalized(self):
        rows = fixture()
        later = copy.deepcopy(rows[0])
        later.update(before_ns=20, after_ns=21)
        later['kib']['VmHWM'] = 98
        later['status'] = later['status'].replace('VmHWM: 100', 'VmHWM: 98')
        del later['high_water_snapshot']
        rows.insert(1, later)
        result = summarize(rows)
        self.assertEqual(result['peak_hwm_kib'], 100)
        self.assertEqual(result['observed_hwm_decreases'],
            [dict(previous_row=0, row=1, previous_kib=100, observed_kib=98)])
        self.assertEqual(rows[1]['kib']['VmHWM'], 98)


if __name__ == '__main__':
    unittest.main()
