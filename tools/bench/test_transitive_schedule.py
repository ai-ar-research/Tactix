import io
import unittest

from transitive_schedule import FIELDS, read_schedule, sorted_under_recorded_keys


class TransitiveScheduleTests(unittest.TestCase):
    def log(self, after=None):
        before = ['0\t4\t936\t1\t0\t2\t99\t3ff0000000000000\t17\t0\n',
                  '1\t5\t937\t1\t0\t2\t99\t3ff0000000000000\t17\t0\n']
        if after is None:
            after = [before[1].replace('1\t5', '0\t5', 1), before[0].replace('0\t4', '1\t4', 1)]
        frame = lambda stage, rows: '# snapshot\t'+stage+'\t7\t9\t1\t11\t2\n# '+'\t'.join(FIELDS)+'\n'+''.join(rows)+'# end\n'
        return '# transitive-schedule\t1\n'+frame('before', before)+frame('after', after)

    def parse(self, text):
        return read_schedule(iter(io.StringIO(text)))

    def test_equal_key_permutation_is_valid(self):
        snapshots = self.parse(self.log())
        self.assertEqual(len(snapshots), 2)
        self.assertTrue(sorted_under_recorded_keys(snapshots[1]))

    def test_repeated_invocations_at_same_conflict(self):
        self.assertEqual(len(self.parse(self.log()+self.log().split('\n', 1)[1])), 4)

    def test_reports_unsorted_output_without_rejecting_permutation(self):
        text = self.log().replace('937\t1\t0\t2\t99\t3ff0000000000000', '937\t1\t0\t2\t99\t4000000000000000')
        self.assertFalse(sorted_under_recorded_keys(self.parse(text)[1]))

    def test_rejects_changed_key_or_member(self):
        for old, new in (('0\t5\t937', '0\t5\t938'), ('0\t5\t937\t1', '0\t5\t937\t0')):
            with self.assertRaises(AssertionError):
                self.parse(self.log().replace(old, new))

    def test_rejects_schema_truncation_and_orphan_end(self):
        for text in (self.log().replace('schedule\t1', 'schedule\t2'), self.log()[:-1], self.log()+'# end\n',
                     self.log().replace('# end\n', '', 1)):
            with self.assertRaises(AssertionError):
                self.parse(text)

    def test_rejects_bad_frame_identity_and_ordinal(self):
        for old, new in (('after\t7', 'after\t8'), ('after', 'before'),
                         ('0\t5\t937', '2\t5\t937'), ('\t1\t11\t2\n', '\t1\t11\t3\n')):
            with self.assertRaises(AssertionError):
                self.parse(self.log().replace(old, new))

    def test_rejects_bad_second_idx_and_score_bits(self):
        for old, new in (('\t5\t937\t1\t0\t2', '\t5\t937\t1\t0\t3'),
                         ('3ff0000000000000', 'nan'), ('\t17\t0\n', '\t17\t2\n')):
            with self.assertRaises(AssertionError):
                self.parse(self.log().replace(old, new))


if __name__ == '__main__':
    unittest.main()
