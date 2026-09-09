import unittest
from score_history import FIELDS, read, compare


class ScoreHistoryTests(unittest.TestCase):
    def content(self, *rows):
        return '# tactix-inprocess-score\t1\n# ' + '\t'.join(FIELDS) + '\n' + '\n'.join(rows) + '\n'

    def row(self, *, label='reorder_begin', conflicts=10, decisions=20, score='1.5', position='0', value=0, active=1):
        return f'tactix\t{label}\t{conflicts}\t{decisions}\t7\t{score}\t{position}\t{value}\t{active}'

    def parse(self, *rows):
        return read(self.content(*rows), solver='tactix', external=7)

    def test_complete_and_absent_position(self):
        rows = self.parse(self.row(), self.row(position='-'))
        self.assertEqual(rows[0]['score'], 1.5)
        self.assertIsNone(rows[1]['position'])

    def test_schema_and_identity(self):
        text = self.content(self.row())
        for bad in (text.rstrip('\n'), text + '\n', text.replace('score\t1', 'score\t2'),
                    text.replace('reorder_begin', ''), text.replace('\t7\t', '\t8\t'),
                    text.rsplit('\t', 1)[0] + '\n'):
            with self.assertRaises(ValueError):
                read(bad, solver='tactix', external=7)

    def test_numeric_and_flags(self):
        for kwargs in ({'score': 'nan'}, {'score': 'inf'}, {'score': '-1'}, {'position': '-1'},
                       {'position': '4294967295'}, {'value': 2}, {'active': 2},
                       {'conflicts': -1}, {'decisions': -1}):
            with self.assertRaises(ValueError):
                self.parse(self.row(**kwargs))

    def test_counter_reset_and_regression(self):
        self.assertEqual(len(self.parse(self.row(conflicts=0, decisions=20), self.row(conflicts=0, decisions=0))), 2)
        for later in (self.row(conflicts=9, decisions=21), self.row(conflicts=11, decisions=19)):
            with self.assertRaises(ValueError):
                self.parse(self.row(), later)

    def test_repeated_sites_and_unmatched(self):
        a = self.parse(self.row(), self.row(score='2'), self.row(label='candidate_only'))
        b = self.parse(self.row(), self.row(score='3'), self.row(label='previous_only'))
        out = compare(a, b, before=(10, 20))
        self.assertEqual(out['common_sites'], 2)
        self.assertEqual(out['differences']['score']['first']['key'][-1], 1)
        self.assertEqual(len(out['unmatched']['candidate']), 1)
        self.assertEqual(len(out['unmatched']['previous']), 1)

    def test_categories_and_exact_cutoff(self):
        a = self.parse(self.row(), self.row(decisions=21))
        b = self.parse(self.row(position='-', value=1, active=0), self.row(decisions=21, score='3'))
        out = compare(a, b, before=(10, 20))
        self.assertEqual(out['differences']['score']['count'], 0)
        for key in ('position', 'value', 'active'):
            self.assertEqual(out['differences'][key]['count'], 1)

    def test_empty_and_reordered_alignment(self):
        with self.assertRaises(ValueError):
            self.parse()
        a = self.parse(self.row(label='a'), self.row(label='b'))
        b = self.parse(self.row(label='b'), self.row(label='a'))
        with self.assertRaises(ValueError):
            compare(a, b, before=(10, 20))
        with self.assertRaises(ValueError):
            compare(a, self.parse(self.row(label='c')), before=(10, 20))


if __name__ == '__main__':
    unittest.main()
