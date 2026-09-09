import tempfile
import unittest
from pathlib import Path

from decision_overlap import threshold_overlap


class DecisionOverlapTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'decisions.tsv'

    def overlap(self, text, stop=10):
        self.path.write_bytes(text)
        return threshold_overlap(self.path, stop)

    def test_no_decision_at_exact_threshold(self):
        self.assertEqual(self.overlap(b'9\t23\t1\t-7\t0\n'), b'')

    def test_one_complete_overlap_row(self):
        tail = b'10\t24\t2\t8\t0\n'
        self.assertEqual(self.overlap(b'9\t23\t1\t-7\t0\n' + tail), tail)

    def test_all_threshold_rows_not_just_last(self):
        tail = b'10\t24\t2\t8\t0\n10\t25\t3\t-9\t0\n'
        self.assertEqual(self.overlap(b'9\t23\t1\t-7\t0\n' + tail), tail)

    def test_entire_trace_at_threshold(self):
        tail = b'0\t1\t1\t8\t0\n0\t2\t2\t-9\t0\n'
        self.assertEqual(self.overlap(tail, 0), tail)

    def test_empty_and_truncated_rejected(self):
        for text in (b'', b'10\t24\t2\t8\t0'):
            with self.assertRaises(AssertionError): self.overlap(text)

    def test_outside_threshold_and_bad_schema_rejected(self):
        for text in (b'11\t24\t2\t8\t0\n', b'-1\t24\t2\t8\t0\n', b'10\t24\n'):
            with self.assertRaises(AssertionError): self.overlap(text)
        with self.assertRaises(ValueError): self.overlap(b'10\tx\t2\t8\t0\n')

    def test_invalid_threshold_rejected(self):
        for threshold in (-1, True, 1.5):
            with self.assertRaises(AssertionError): self.overlap(b'0\t1\t1\t8\t0\n', threshold)


if __name__ == '__main__':
    unittest.main()
