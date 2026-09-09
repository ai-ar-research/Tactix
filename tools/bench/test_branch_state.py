import unittest
from branch_state import read_heap, compare_heaps, read_choices

HEADER = ['# tactix-heap-set\t1', '# external\tposition\tscore\tassigned\tactive']


class BranchStateTests(unittest.TestCase):
    def choice(self, **changes):
        fields = ['10', '20', '3', '-5', '7', '0', '4294967295', '2', '1.25',
                  'unassigned', '1', '40', '5', '40', '30', '3', '8', '31']
        for index, value in changes.items():
            fields[int(index)] = str(value)
        return '\t'.join(fields)

    def parse(self, rows):
        return read_choices(rows, conflict=10, wanted=7)

    def test_heap_comparison_categories(self):
        a = read_heap(HEADER + ['1\t0\t2\t0\t1', '2\t1\t1\t0\t1'])
        b = read_heap(HEADER + ['2\t0\t3\t1\t0', '3\t1\t1\t0\t1'])
        diff = compare_heaps(a, b)
        self.assertEqual(diff['candidate_only'], [1])
        self.assertEqual(diff['previous_only'], [3])
        for key in ('position', 'score', 'assigned', 'active'):
            self.assertEqual([r['variable'] for r in diff[key]], [2])

    def test_heap_empty_and_headers(self):
        self.assertEqual(read_heap(HEADER), {})
        for rows in ([], HEADER[:1], ['bad', HEADER[1]]):
            with self.assertRaises(ValueError):
                read_heap(rows)

    def test_heap_malformed(self):
        for row in ('1\t1\t1\t0\t1', '0\t0\t1\t0\t1', '1\t0\t1\t2\t1',
                    '1\t0\tnan\t0\t1', '1\t0\tinf\t0\t1', '1\t0\t-1\t0\t1', '1\t0'):
            with self.assertRaises(ValueError):
                read_heap(HEADER + [row])
        with self.assertRaises(ValueError):
            read_heap(HEADER + ['1\t0\t1\t0\t1', '1\t1\t1\t0\t1'])

    def test_choice_unassigned_and_reasons(self):
        self.assertEqual(self.parse([self.choice()])[0]['value'], 0)
        for reason in ('decision', 'unit', 'binary:-3', 'large:2,-7,5'):
            row = self.choice(**{'5': -1, '6': 0 if reason == 'unit' else 2, '9': reason})
            self.assertEqual(self.parse([row])[0]['reason'], reason)

    def test_choice_order_and_window(self):
        self.assertEqual(len(self.parse([self.choice(), self.choice(**{'1': 21})])), 2)
        for changes in ({'0': 11}, {'4': 8}, {'3': 0}, {'10': 2}, {'5': 2}, {'1': -1}):
            with self.assertRaises(ValueError):
                self.parse([self.choice(**changes)])
        for decision in (19, 20, 22):
            with self.assertRaises(ValueError):
                self.parse([self.choice(), self.choice(**{'1': decision})])

    def test_choice_assignment_consistency(self):
        for changes in ({'9': 'decision'}, {'6': 0}, {'5': 1, '6': 4, '9': 'unit'},
                        {'5': 1, '6': 2, '9': 'unit'},
                        {'5': 1, '6': 0, '9': 'unknown'}, {'8': 'nan'}):
            with self.assertRaises(ValueError):
                self.parse([self.choice(**changes)])
        for reason in ('binary:0', 'binary:1,2', 'large:', 'large:1,0', 'unassigned'):
            with self.assertRaises(ValueError):
                self.parse([self.choice(**{'5': 1, '6': 1, '9': reason})])

    def test_choice_missing_or_truncated(self):
        for rows in ([], [self.choice().rsplit('\t', 1)[0]], [self.choice() + '\textra']):
            with self.assertRaises(ValueError):
                self.parse(rows)


if __name__ == '__main__':
    unittest.main()
