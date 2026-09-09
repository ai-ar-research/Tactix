import io
import unittest

from vivify_trace import CANDIDATE_FIELDS, ROUND_FIELDS, read_candidates, read_rounds


class VivifyTraceTests(unittest.TestCase):
    def round_row(self, stage, **updates):
        row = dict(solver='tactix', seq=int(stage == 'end'), stage=stage,
                   conflicts=7, scope=2, scheduled=2, tried=int(stage == 'end'),
                   produced=0, ticks=10 if stage == 'begin' else 15, limit=14, offset=0)
        row.update(updates)
        return '\t'.join(str(row[k]) for k in ROUND_FIELDS) + '\n'

    def candidate(self, event, **updates):
        seq = {'vivify_candidate': 0, 'vivify_candidate_end': 1, 'vivify_budget_stop': 2}[event]
        row = dict(solver='tactix', seq=seq, event=event, conflicts=7, scope=2,
                   ordinal=int(seq == 2), ticks=10 if seq == 0 else 15, limit=14,
                   ref=3, position=1, size=3, redundant=1, glue=2, priority=1,
                   garbage=0, clause='-3,1,2', pose='2,-3,1')
        row.update(updates)
        return '\t'.join(str(row[k]) for k in CANDIDATE_FIELDS) + '\n'

    def round_log(self):
        return '# tactix-vivify-round\t1\n# ' + '\t'.join(ROUND_FIELDS) + '\n' + self.round_row('begin') + self.round_row('end')

    def candidate_log(self):
        return '# tactix-vivify-candidate\t1\n# ' + '\t'.join(CANDIDATE_FIELDS) + '\n' + ''.join(self.candidate(e) for e in ('vivify_candidate', 'vivify_candidate_end', 'vivify_budget_stop'))

    def parse(self, candidate=None, rounds=None):
        frames = read_rounds(io.StringIO(rounds or self.round_log()), 7)
        return list(read_candidates(io.StringIO(candidate or self.candidate_log()), frames))

    def test_budget_cut_and_pose(self):
        result = self.parse()
        self.assertEqual(len(result), 3)
        self.assertEqual(result[0]['clause'], (-3, 1, 2))
        self.assertEqual(result[-1]['ordinal'], 1)

    def test_completed_round_and_empty_round(self):
        rounds = self.round_log().replace('\t2\t1\t0\t15', '\t1\t1\t0\t15').replace('\t2\t0\t0\t10', '\t1\t0\t0\t10')
        candidates = self.candidate_log().replace(self.candidate('vivify_budget_stop'), '')
        self.assertEqual(len(self.parse(candidates, rounds)), 2)
        header = self.round_log().split('\n', 2)[:2]
        rounds = '\n'.join(header) + '\n' + self.round_row('begin', scheduled=0) + self.round_row('end', scheduled=0, tried=0)
        self.assertEqual(self.parse('\n'.join(self.candidate_log().split('\n')[:2]) + '\n', rounds), [])

    def test_repeated_scope_and_reset_ordinal(self):
        rounds = self.round_log() + self.round_row('begin', seq=2) + self.round_row('end', seq=3)
        candidates = self.candidate_log() + ''.join(self.candidate(e, seq=i + 3) for i, e in enumerate(('vivify_candidate', 'vivify_candidate_end', 'vivify_budget_stop')))
        self.assertEqual([r['round'] for r in self.parse(candidates, rounds)], [0, 0, 0, 1, 1, 1])

    def test_empty_round_before_immediate_budget_cut(self):
        header = '\n'.join(self.round_log().split('\n')[:2]) + '\n'
        rounds = header + self.round_row('begin', scheduled=0) + self.round_row('end', scheduled=0, tried=0)
        rounds += self.round_row('begin', seq=2, ticks=15) + self.round_row('end', seq=3, tried=0)
        candidates = '\n'.join(self.candidate_log().split('\n')[:2]) + '\n'
        candidates += self.candidate('vivify_budget_stop', seq=0, ordinal=0)
        self.assertEqual([r['round'] for r in self.parse(candidates, rounds)], [1])

    def test_reject_schema_truncation_width_and_sequence(self):
        text = self.candidate_log()
        for bad in (text.replace('candidate\t1\n', 'candidate\t2\n'), text[:-1], text + '\n', text.replace('tactix\t0\t', 'tactix\t4\t'), text.replace('\t2,-3,1\n', '\n')):
            with self.subTest(bad=bad), self.assertRaises((AssertionError, ValueError)):
                self.parse(bad)

    def test_reject_round_framing_and_counts(self):
        text = self.round_log()
        for bad in (text.replace(self.round_row('end'), ''), text.replace(self.round_row('begin'), ''), text.replace(self.round_row('end'), self.round_row('end', tried=3)), text.replace(self.round_row('end'), self.round_row('end', ticks=9))):
            with self.subTest(bad=bad), self.assertRaises(AssertionError):
                self.parse(rounds=bad)

    def test_reject_candidate_identity_and_effort(self):
        for changes in ({'conflicts': 8}, {'scope': 3}, {'limit': 15}, {'ticks': 9}, {'ordinal': 1}, {'redundant': 2}, {'ref': -1}):
            text = self.candidate_log().replace(self.candidate('vivify_candidate'), self.candidate('vivify_candidate', **changes))
            with self.subTest(changes=changes), self.assertRaises(AssertionError):
                self.parse(text)

    def test_reject_clause_multiset_and_zero_literals(self):
        for changes in ({'clause': '-3,1,3'}, {'pose': '0,-3,1'}, {'size': 4}, {'clause': '2,1,-3'}):
            text = self.candidate_log().replace(self.candidate('vivify_candidate'), self.candidate('vivify_candidate', **changes))
            with self.subTest(changes=changes), self.assertRaises(AssertionError):
                self.parse(text)

    def test_reject_missing_end_extra_row_or_changed_ref(self):
        text = self.candidate_log()
        for bad in (text.replace(self.candidate('vivify_candidate_end'), ''), text + self.candidate('vivify_candidate', seq=3), text.replace(self.candidate('vivify_candidate_end'), self.candidate('vivify_candidate_end', ref=4))):
            with self.subTest(bad=bad), self.assertRaises(AssertionError):
                self.parse(bad)

    def test_reject_invalid_budget_stop(self):
        for changes in ({'ordinal': 2}, {'ticks': 14}, {'scope': 3}):
            text = self.candidate_log().replace(self.candidate('vivify_budget_stop'), self.candidate('vivify_budget_stop', **changes))
            with self.subTest(changes=changes), self.assertRaises(AssertionError):
                self.parse(text)

    def test_allows_termination_without_budget_stop(self):
        self.assertEqual(len(self.parse(self.candidate_log().replace(self.candidate('vivify_budget_stop'), ''))), 2)


if __name__ == '__main__':
    unittest.main()
