import unittest

from queue_bump_trace import read_bumps


class QueueBumpTraceTests(unittest.TestCase):
    row = '10\t20\t0\t7\t99\t0\t8\t9\t98\t101\t-1\n'

    def read(self, content, **kwargs):
        return read_bumps(content, window=(10, 11), cap=10, **kwargs)

    def test_raw_state_and_duplicates_preserved(self):
        rows = self.read(self.row * 2)
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0], rows[1])
        self.assertEqual(rows[0]['previous'], 0)
        self.assertEqual(rows[0]['stamp'], 99)
        self.assertEqual(rows[0]['search_stamp'], 98)
        self.assertEqual(rows[0]['value'], -1)

    def test_empty_truncated_width_and_integer(self):
        for content in ('', self.row[:-1], self.row.replace('99\t', ''), self.row.replace('99', 'x')):
            with self.subTest(content=content), self.assertRaises(ValueError):
                self.read(content)

    def test_identity_value_and_window(self):
        for index, value in ((0, 9), (1, -1), (2, 1), (3, 0), (4, -1), (10, 2)):
            cells = self.row.rstrip('\n').split('\t'); cells[index] = str(value)
            with self.subTest(index=index), self.assertRaises(ValueError):
                self.read('\t'.join(cells) + '\n')

    def test_monotone_counters(self):
        self.assertEqual(len(self.read(self.row + self.row.replace('10\t20', '11\t21'))), 2)
        for second in (self.row.replace('10\t20', '10\t19'), self.row):
            first = self.row.replace('10\t20', '11\t21')
            with self.assertRaises(ValueError): self.read(first + second)

    def test_invalid_scope_and_cap(self):
        for window, cap in (((11, 10), 10), ((-1, 10), 10), ((10,), 10), ((10, 11), 0), ((10, 11), 1)):
            with self.subTest(window=window, cap=cap), self.assertRaises(ValueError):
                read_bumps(self.row, window=window, cap=cap)


if __name__ == '__main__':
    unittest.main()
