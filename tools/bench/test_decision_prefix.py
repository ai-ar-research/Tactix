import tempfile
import unittest
from pathlib import Path

from decision_prefix import FIELDS, first_stream_difference, read_prefix, validate_common, validate_stream_accounting


class DecisionPrefixTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'prefix.tsv'
        self.values = ['tactix', '300000', '18446744073709414842', '74393487',
                       '74530261', 'b16c9d74ee88e6da', '40102204', '1',
                       'd6521123d1b0ecf7', '489816', '-81843', '147451', '45166']

    def write_record(self):
        self.path.write_text('# tactix-decision-prefix\t1\n# ' + '\t'.join(FIELDS)
                             + '\n' + '\t'.join(self.values) + '\n')
        return read_prefix(self.path)

    def validate(self, record):
        validate_common(record, stop=300000, common=40102204, digest='d6521123d1b0ecf7')

    def test_count_is_distinct_from_reached_boolean(self):
        record = self.write_record()
        self.validate(record)
        self.assertEqual(record['common_reached'], 1)
        self.assertEqual(record['common'], 40102204)
        self.assertEqual(record['start_decisions'], 18446744073709414842)

    def test_rejects_wrong_boundary_count_digest_and_truncation(self):
        original = self.write_record()
        for field, value in [('conflict', 299999), ('common', 1), ('common_reached', 0),
                             ('count', 40102203), ('common_hash', '0000000000000000')]:
            with self.subTest(field=field), self.assertRaises(AssertionError):
                self.validate(dict(original, **{field: value}))

    def test_rejects_nonboolean_reached(self):
        self.values[7] = '40102204'
        with self.assertRaises(AssertionError):
            self.write_record()

    def test_rejects_duplicate_or_malformed_schema(self):
        self.write_record()
        original = self.path.read_text()
        for text in [original + '\t'.join(self.values) + '\n',
                     original.replace('common_reached', 'reached'),
                     original.replace('prefix\t1', 'prefix\t2'),
                     original.rsplit('\t', 1)[0] + '\n']:
            with self.subTest(text=text), self.assertRaises(AssertionError):
                self.path.write_text(text)
                read_prefix(self.path)

    def compare_streams(self, left, right, chunk):
        a, b = Path(self.tmp.name) / 'a.tsv', Path(self.tmp.name) / 'b.tsv'
        a.write_text(left)
        b.write_text(right)
        return first_stream_difference(a, b, chunk_bytes=chunk)

    def test_stream_equality_across_every_chunk_boundary(self):
        text = '0\t1\t1\t-12345\t0\n1\t2\t2\t67\t1\n'
        for chunk in range(1, len(text) + 2):
            self.assertEqual(self.compare_streams(text, text, chunk),
                             dict(matching_rows=2, left=None, right=None, complete_equal=True))

    def test_stream_difference_preserves_entire_straddling_row(self):
        prefix = '0\t1\t1\t-12345\t0\n'
        a, b = '1\t2\t2\t67\t1\n', '1\t2\t2\t-67\t1\n'
        for chunk in range(1, len(prefix + a) + 2):
            self.assertEqual(self.compare_streams(prefix + a, prefix + b, chunk),
                             dict(matching_rows=1, left=[1,2,2,67,1], right=[1,2,2,-67,1], complete_equal=False))

    def test_stream_unequal_lengths_and_empty(self):
        text = '0\t1\t1\t3\t0\n'
        for chunk in range(1, len(text) + 2):
            for a, b in [('', text), (text, text + text), (text, ''), (text + text, text)]:
                got = self.compare_streams(a, b, chunk)
                self.assertEqual(got['matching_rows'], min(a.count('\n'), b.count('\n')))
                self.assertEqual(got['left'] is None, len(a) < len(b))
                self.assertEqual(got['right'] is None, len(b) < len(a))
        self.assertTrue(self.compare_streams('', '', 1)['complete_equal'])

    def test_stream_rejects_truncation_and_wrong_columns(self):
        for text in ['0\t1\t1\t3\t0', '0\t1\n', '# wrong header\n']:
            with self.subTest(text=text), self.assertRaises(AssertionError):
                self.compare_streams(text, text, 2)


    def test_cumulative_rows_survive_solver_counter_reset(self):
        # Actual Rank45 shape:386 more cumulative rows than the final counter.
        record = dict(conflict=100000, count=182035, end_decisions=181649,
                      start_decisions=(181649-182035) % (1 << 64), first=5322, last=5735)
        first, last = [0,1,1,5322,0], [100000,181649,6,5735,0]
        self.assertNotEqual(first[1], (record['start_decisions']+1) % (1 << 64))
        validate_stream_accounting(record, first, last, 182035)

    def test_full_stream_accounting_rejects_wrong_edges_and_counts(self):
        record = dict(conflict=2, count=3, end_decisions=3,
                      start_decisions=0, first=7, last=-9)
        first, last = [0,1,1,7,0], [2,3,2,-9,0]
        validate_stream_accounting(record, first, last, 3)
        for field, value in [('start_decisions',1), ('end_decisions',4),
                             ('first',-7), ('last',9), ('conflict',1), ('count',4)]:
            with self.subTest(field=field), self.assertRaises(AssertionError):
                validate_stream_accounting(dict(record, **{field:value}), first, last, 3)
        with self.assertRaises(AssertionError):
            validate_stream_accounting(record, first[:4], last, 3)


if __name__ == '__main__':
    unittest.main()
