import io
import unittest
from perf_symbol_cost import read_report, select


HEAD = """# Total Lost Samples: 0
# Samples: 10K of event 'cycles:u'
# Event count (approx.): 1000000
# Children Self Shared Object Symbol IPC [IPC Coverage]
"""
ROW = '20.00% 5.00% candidate [.] parent(int) - -\n'


class PerfSymbolCostTests(unittest.TestCase):
    def read(self, text=HEAD + ROW, inclusive=True):
        return read_report(io.StringIO(text), inclusive)

    def test_calltrees_are_not_extra_rows(self):
        r = self.read(HEAD + ROW + '|--20.00%--parent(int)\n| |\n' +
                      '  |--5.00%--child()\n')
        self.assertEqual(len(r['rows']), 1)
        self.assertEqual(select(r, 'candidate', ['parent(int)'])['estimated_cycles'], 200000)
        self.assertEqual(select(r, 'candidate', ['parent(int)'], False)['estimated_cycles'], 50000)

    def test_whitespace_and_rounding(self):
        r = self.read((HEAD + ROW).replace('candidate ', 'candidate' + ' ' * 10000))
        self.assertEqual(select(r, 'candidate', ['parent(int)'])['rounding_cycles'], 50)

    def test_self_report(self):
        head = HEAD.replace('Children Self', 'Overhead')
        r = self.read(head + ROW.replace('20.00% ', ''), False)
        self.assertEqual(select(r, 'candidate', ['parent(int)'])['estimated_cycles'], 50000)

    def test_incomplete_or_wrong_metadata(self):
        for before, after in [('# Total Lost Samples: 0\n', ''),
                              ('Samples: 0', 'Samples: 1'),
                              ('1000000', '0'), ('1000000', '-1'),
                              ("cycles:u", 'instructions:u'),
                              ('# Children Self Shared Object Symbol IPC [IPC Coverage]\n', '')]:
            with self.subTest(before=before, after=after), self.assertRaises(ValueError):
                self.read((HEAD + ROW).replace(before, after))
        for text in [HEAD, HEAD + HEAD + ROW]:
            with self.assertRaises(ValueError):
                self.read(text)

    def test_malformed_or_duplicate_rows(self):
        for rows in [ROW + ROW, ROW.replace('5.00%', '21.00%'),
                     ROW.replace('20.00%', '101.00%'), ROW.replace(' - -', ''),
                     ROW.replace('20.00%', '20%'), ROW + ROW.replace('20.00%', '-1.00%'),
                     ROW + ROW.replace('20.00%', 'nan%'), ROW.replace('20.00%', '20.0%')]:
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                self.read(HEAD + rows)

    def test_exact_selection_no_lambdas_or_absent_zero(self):
        r = self.read(HEAD + ROW + ROW.replace('parent(int)', 'parent(int)::lambda()'))
        self.assertEqual(select(r, 'candidate', ['parent(int)'])['percent'], 20)
        for symbols in [[], ['parent'], ['parent(int)', 'parent(int)']]:
            with self.assertRaises(ValueError):
                select(r, 'candidate', symbols)
        with self.assertRaises(ValueError):
            select(r, 'other', ['parent(int)'])

    def test_wrong_table_kind(self):
        with self.assertRaises(ValueError):
            self.read(HEAD + ROW, False)


if __name__ == '__main__':
    unittest.main()
