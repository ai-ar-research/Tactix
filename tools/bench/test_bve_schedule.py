import unittest
from bve_schedule import parse


class ScheduleTests(unittest.TestCase):
    def example(self):
        return ['# bve-schedule-observer\t1\n', 'tactix\tscheduled=2|bound=8\n',
                'member\texternal=4\tposition=0\tpositive=2\tnegative=3\tscore=414e845f80000000\n',
                'member\texternal=9\tposition=1\tpositive=0\tnegative=1\tscore=0000000000000000\n']

    def test_complete(self):
        header, members = parse(self.example())
        self.assertEqual(header['scheduled'], '2')
        self.assertEqual(members[1]['positive'], '0')

    def test_empty(self):
        self.assertEqual(parse(['# bve-schedule-observer\t1', 'kissat\tscheduled=0'])[1], [])

    def test_missing_and_extra_member(self):
        for rows in (self.example()[:-1], self.example() + self.example()[-1:]):
            with self.assertRaises(ValueError):
                parse(rows)

    def test_duplicate_and_position(self):
        for before, after in (('external=9', 'external=4'), ('position=1', 'position=2'),
                              ('external=9', 'external=0')):
            rows = self.example()
            rows[-1] = rows[-1].replace(before, after)
            with self.assertRaises(ValueError):
                parse(rows)

    def test_schema(self):
        for before, after in (('negative=1', 'negative=-1'), ('negative=1', 'extra=1'),
                              ('negative=1', 'positive=1'), ('0000000000000000', '0'),
                              ('member\t', 'selected\t')):
            rows = self.example()
            rows[-1] = rows[-1].replace(before, after)
            with self.assertRaises(ValueError):
                parse(rows)

    def test_header(self):
        for row in ('bad', 'tactix\tscheduled=2|scheduled=2', 'unknown\tscheduled=2',
                    'tactix\tbound=8', 'tactix\tscheduled=-2'):
            rows = self.example()
            rows[1] = row
            with self.assertRaises(ValueError):
                parse(rows)
        with self.assertRaises(ValueError):
            parse([])


if __name__ == '__main__':
    unittest.main()
