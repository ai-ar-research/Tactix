import datetime as dt
import unittest
from session_deadline import phase


class SessionDeadlineTests(unittest.TestCase):
    def setUp(self):
        self.deadline = dt.datetime.fromisoformat('2026-09-10T06:00:00-04:00')

    def test_working(self):
        self.assertEqual(phase(self.deadline-dt.timedelta(minutes=16), self.deadline), 'working')

    def test_wind_down_equality(self):
        self.assertEqual(phase(self.deadline-dt.timedelta(minutes=15), self.deadline), 'wind_down')

    def test_cutoff_equality_and_later(self):
        for delta in (dt.timedelta(), dt.timedelta(hours=1)):
            self.assertEqual(phase(self.deadline+delta, self.deadline), 'cutoff')

    def test_utc_conversion(self):
        self.assertEqual(phase(dt.datetime.fromisoformat('2026-09-10T10:00:00+00:00'), self.deadline), 'cutoff')

    def test_invalid_timezones(self):
        with self.assertRaises(ValueError):
            phase(self.deadline.replace(tzinfo=None), self.deadline)
        with self.assertRaises(ValueError):
            phase(self.deadline, self.deadline.replace(tzinfo=None))

    def test_invalid_interval(self):
        with self.assertRaises(ValueError):
            phase(self.deadline, self.deadline, -1)


if __name__ == '__main__':
    unittest.main()
