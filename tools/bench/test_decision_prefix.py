import tempfile
import unittest
from pathlib import Path

from decision_prefix import FIELDS, read_prefix, validate_common


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


if __name__ == '__main__':
    unittest.main()
