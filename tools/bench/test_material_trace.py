import unittest
from material_trace import parse_material

ROW = ('kissat\tconflicts=9\tdecisions=17\tcount=2\tsaved=1\texternal=-17'
       '\tsource=add_binary\tbefore=0\tafter=1\tfixed=0\tactive=1\tprobing=0\n')


class MaterialTraceTests(unittest.TestCase):
    def test_complete_signed_records(self):
        rows = parse_material(ROW + ROW.replace('count=2', 'count=3').replace('external=-17', 'external=23'), 2, 3, 9)
        self.assertEqual([r['external'] for r in rows], [-17, 23])

    def test_exact_large_integers(self):
        row = ROW.replace('decisions=17', 'decisions=9007199254740993')
        self.assertEqual(parse_material(row, 2, 2, 9)[0]['decisions'], 9007199254740993)

    def test_missing_duplicate_or_truncated(self):
        for text in ('', ROW[:-1], ROW + ROW, ROW + '\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_material(text, 2, 2, 9)

    def test_invalid_bounds(self):
        for bounds in ((True, 2, 9), (3, 2, 9), (-1, 2, 9), (2, 2, 1 << 64), (2.0, 2, 9)):
            with self.subTest(bounds=bounds), self.assertRaises(ValueError):
                parse_material(ROW, *bounds)

    def test_wrong_fields_or_sources(self):
        for text in (ROW.replace('kissat', 'tactix'), ROW.replace('saved=', 'count='),
                     ROW.replace('add_binary', 'add binary'), ROW.replace('\tsource=add_binary', ''),
                     ROW.rstrip() + '\textra=0\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_material(text, 2, 2, 9)

    def test_invalid_numbers(self):
        for field in ('external=0', 'external=+17', 'external=2147483648', 'external=-2147483648'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                parse_material(ROW.replace('external=-17', field), 2, 2, 9)
        for value in ('-1', '1.0', '18446744073709551616', '９'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                parse_material(ROW.replace('decisions=17', 'decisions=' + value), 2, 2, 9)

    def test_invalid_transition_or_flag(self):
        for old, new in (('before=0', 'before=1'), ('after=1', 'after=0'),
                         ('fixed=0', 'fixed=2'), ('saved=1', 'saved=3')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                parse_material(ROW.replace(old, new), 2, 2, 9)

    def test_wrong_counter_or_conflict_window(self):
        for text in (ROW.replace('count=2', 'count=3'), ROW.replace('conflicts=9', 'conflicts=10')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_material(text, 2, 2, 9)

    def test_nonmonotone_history(self):
        for key, value in (('conflicts=9', 'conflicts=8'), ('decisions=17', 'decisions=16'), ('saved=1', 'saved=0')):
            with self.subTest(key=key), self.assertRaises(ValueError):
                parse_material(ROW + ROW.replace('count=2', 'count=3').replace(key, value), 2, 3, 9)


if __name__ == '__main__':
    unittest.main()
