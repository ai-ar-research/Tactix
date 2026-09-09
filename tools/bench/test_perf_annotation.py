import unittest
from perf_annotation import parse_periods


FIXTURE = ''' Period      | Source code & Disassembly of candidate for cycles:u (3 samples, percent: local period)
----------------
             :
             : 3      Disassembly of section .text:
             : 5      0000000000000100 <example(int)>:
           7 :   100: mov (%rax),%edx
           0 :   102: test $0x1,%dl
          11 :   106: retq
'''


class PeriodAnnotationTests(unittest.TestCase):
    def parse(self, text=FIXTURE):
        return parse_periods(text, 'example(int)')

    def test_exact_period_and_half_open_range(self):
        a = self.parse()
        self.assertEqual((a.symbol, a.samples, a.total_period), ('example(int)', 3, 18))
        self.assertEqual(a.range_period(0x100, 0x106), 7)
        self.assertEqual(a.range_period(0x102, 0x106), 0)

    def test_integer_precision(self):
        a = self.parse(FIXTURE.replace('7 :', '9007199254740993 :'))
        self.assertEqual(a.total_period, 9007199254741004)

    def test_reject_wrong_mode_or_empty_samples(self):
        for text in ('', FIXTURE.replace('Period', 'Percent'),
                     FIXTURE.replace('3 samples', '0 samples'),
                     FIXTURE.replace('cycles:u', 'instructions:u')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.parse(text)

    def test_reject_wrong_or_repeated_symbol(self):
        for text in (FIXTURE.replace('example(int)', 'other()'), FIXTURE + FIXTURE.splitlines()[4] + '\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.parse(text)

    def test_reject_bad_instruction_rows(self):
        for old, new in (('7 :', '-7 :'), ('7 :', '7.0 :'), ('100: mov', '101: mov'),
                         ('102: test', '100: test'), ('106: retq', '101: retq')):
            with self.subTest(old=old, new=new), self.assertRaises(ValueError):
                self.parse(FIXTURE.replace(old, new))

    def test_reject_missing_and_unrecognized_content(self):
        for text in (FIXTURE[:FIXTURE.index('           7 :')],
                     FIXTURE.replace('7 :', '0 :').replace('11 :', '0 :'),
                     FIXTURE + 'warning: incomplete\n',
                     '\n'.join(line for line in FIXTURE.splitlines() if '<example' not in line)):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.parse(text)

    def test_range_boundaries_must_be_exact_and_ordered(self):
        a = self.parse()
        for begin, end in ((0x101, 0x106), (0x100, 0x107), (0x106, 0x100), (0x100, 0x100)):
            with self.subTest(begin=begin, end=end), self.assertRaises(ValueError):
                a.range_period(begin, end)


if __name__ == '__main__':
    unittest.main()
