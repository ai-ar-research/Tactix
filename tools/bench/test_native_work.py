import unittest
from native_work import primary_statistics


class NativeWorkTests(unittest.TestCase):
    def section(self, *rows):
        return ['c ---- [ statistics ] ----', *rows, 'c ---- [ resources ] ----']

    def test_rates_resources_and_other_sections_not_counters(self):
        a = ['c ---- [ profiling ] ----', 'c 2.3 100 % total'] + self.section(
            'c conflicts: 7 3.5 per second', 'c eliminated: 0 ') + ['c maximum-resident-set-size: 500']
        b = [s.replace('3.5', '99.0').replace('500', '900') for s in a]
        self.assertEqual(primary_statistics(a), {'conflicts': 7, 'eliminated': 0})
        self.assertEqual(primary_statistics(a), primary_statistics(b))

    def test_changed_count_is_visible(self):
        self.assertNotEqual(primary_statistics(self.section('c conflicts: 7')),
                            primary_statistics(self.section('c conflicts: 8')))

    def test_duplicate_missing_or_truncated_sections(self):
        good = self.section('c conflicts: 7')
        for lines in ([], good[1:], good[:-1], good + good, self.section()):
            with self.subTest(lines=lines), self.assertRaises(ValueError):
                primary_statistics(lines)

    def test_duplicate_and_malformed_counters(self):
        for rows in (('c conflicts: 7', 'c conflicts: 8'), ('c conflicts: -1',),
                     ('c conflicts: 7.5',), ('c conflicts: x',), ('c stray text',)):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                primary_statistics(self.section(*rows))

    def test_input_schema_and_no_mutation(self):
        for lines in ('c conflicts: 7', [1], None):
            with self.assertRaises(ValueError): primary_statistics(lines)
        lines = self.section('c conflicts: 7'); before = list(lines)
        primary_statistics(lines)
        self.assertEqual(lines, before)


if __name__ == '__main__': unittest.main()
