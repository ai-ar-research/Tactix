import unittest
from congruence_timing import deltas, parse_cumulative


ROW = ('[cong-time] cumulative ms: apply 100 | extract{and 10 index 20 sort 0 xor 5 ite 4} '
       '| rebuild 6 binary{extract 7 units 8 import 9} setup 3 | '
       'closure 15 {canon 2 match 8 (of which proof 1)} | forward 4 | items 30')


class CongruenceTimingTests(unittest.TestCase):
    def test_cumulative_and_deltas(self):
        rows = parse_cumulative(ROW + '\n' + ROW.replace('apply 100', 'apply 120').replace('items 30', 'items 31'), 2)
        change = deltas(rows)
        self.assertEqual(change[0]['index'], 20)
        self.assertEqual(change[1]['apply'], 20)
        self.assertEqual(change[1]['index'], 0)
        self.assertEqual(change[1]['items'], 1)

    def test_unrelated_stderr(self):
        self.assertEqual(parse_cumulative('notice\n' + ROW + '\nother text', 1)[0]['apply'], 100)

    def test_exact_integers(self):
        self.assertEqual(parse_cumulative(ROW.replace('items 30', 'items 9007199254740993'), 1)[0]['items'], 9007199254740993)

    def test_no_rows_or_wrong_count(self):
        for text, count in (('', 1), (ROW, 2), (ROW + '\n' + ROW, 1)):
            with self.subTest(count=count), self.assertRaises(ValueError):
                parse_cumulative(text, count)

    def test_invalid_expected_count(self):
        for count in (0, -1, True, 1.0, '1'):
            with self.subTest(count=count), self.assertRaises(ValueError):
                parse_cumulative(ROW, count)

    def test_malformed_rows_fail_closed(self):
        for text in (ROW.replace('index 20', 'index -1'), ROW.replace('index 20', 'index 1.5'),
                     ROW + ' trailing', ' ' + ROW, ROW[:-5], ROW.replace('sort 0 ', '')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_cumulative(text, 1)

    def test_decreasing_fields(self):
        for field in ('apply 100', 'index 20', 'items 30'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                parse_cumulative(ROW + '\n' + ROW.replace(field, field.split()[0] + ' 0'), 2)

    def test_overlap_is_not_rejected_or_summed(self):
        rows = parse_cumulative(ROW.replace('proof 1', 'proof 8'), 1)
        self.assertEqual(rows[0]['match'], rows[0]['proof'])
        with self.assertRaises(ValueError):
            deltas([{'apply': 2}, {'index': 2}])
        with self.assertRaises(ValueError):
            deltas([{'apply': 2}, {'apply': 1}])


if __name__ == '__main__':
    unittest.main()
