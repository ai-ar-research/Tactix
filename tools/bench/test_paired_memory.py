"""Fixed reporting controls, independent of real solver execution."""
import copy
import unittest

from paired_memory import summarize_memory


def records():
    return [dict(wave=w, arm=a, solve=dict(peak_rss_kb=v, sampled_process_vm_peak_kb=2*v))
            for w, pair in enumerate(((50, 100), (120, 100), (80, 100)))
            for a, v in zip(('candidate', 'previous'), pair)]


class PairedMemoryTests(unittest.TestCase):
    def test_complete_pairs_not_best_of(self):
        result = summarize_memory(records())
        for value in result.values():
            self.assertTrue(value['complete'])
            self.assertEqual(value['paired'], dict(values=[.5, 1.2, .8], min=.5, median=.8, max=1.2))
        self.assertEqual(result['peak_rss_kb']['arms']['candidate'], dict(min=50, median=80, max=120))

    def test_input_order_does_not_change_pairing(self):
        self.assertEqual(summarize_memory(records()), summarize_memory(reversed(records())))

    def test_missing_wave_and_duplicate_rejected(self):
        for rows in ([], records()[:-1], records()+[records()[0]]):
            with self.assertRaises(ValueError): summarize_memory(rows)

    def test_missing_vmpeak_not_zero_or_rss_substitution(self):
        rows = records(); del rows[0]['solve']['sampled_process_vm_peak_kb']
        result = summarize_memory(rows)
        self.assertTrue(result['peak_rss_kb']['complete'])
        self.assertFalse(result['sampled_process_vm_peak_kb']['complete'])
        self.assertEqual(result['sampled_process_vm_peak_kb']['missing'], [(0, 'candidate')])
        self.assertNotIn('paired', result['sampled_process_vm_peak_kb'])

    def test_missing_rss_remains_unknown(self):
        rows = records(); rows[3]['solve']['peak_rss_kb'] = None
        self.assertFalse(summarize_memory(rows)['peak_rss_kb']['complete'])

    def test_invalid_samples_rejected_without_mutation(self):
        for bad in (0, -1, True, 1.5, '50'):
            rows = records(); rows[0]['solve']['peak_rss_kb'] = bad
            before = copy.deepcopy(rows)
            with self.assertRaises(ValueError): summarize_memory(rows)
            self.assertEqual(rows, before)

    def test_invalid_identity_rejected(self):
        for field, value in (('wave', -1), ('wave', 3), ('wave', True), ('arm', 'kissat')):
            rows = records(); rows[0][field] = value
            with self.assertRaises(ValueError): summarize_memory(rows)


if __name__ == '__main__':
    unittest.main()
