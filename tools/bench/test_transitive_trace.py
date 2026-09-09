import io
import unittest

from transitive_trace import FIELDS, binary_drops, read_phase, reduced_pairs


class TransitiveTraceTests(unittest.TestCase):
    def row(self, event, solver='tactix', tick_offset=0, **updates):
        r = dict.fromkeys(FIELDS, 0)
        r.update(solver=solver, event=event, source=3, destination=4,
                 candidate_count=1, out_degree=2, source_active=1,
                 destination_active=1, conflicts=7, ticks_before=10,
                 ticks_after=12, skip_reason='-')
        if event in ('source-begin', 'source-end'):
            r['destination'] = 0
        if event == 'source-end':
            r['candidate_ordinal'] = 1
        if event in ('candidate', 'delete', 'source-end'):
            r['ticks_after'] = 14
        if event == 'candidate':
            r['ticks_before'] = 12
        if event == 'delete':
            r['ticks_before'] = 14
        r['ticks_before'] += tick_offset
        r['ticks_after'] += tick_offset
        r.update(updates)
        return '\t'.join(str(r[k]) for k in FIELDS) + '\n'

    def log(self, solver='tactix', reduced=True, tick_offset=0):
        rows = [self.row('source-begin', solver, tick_offset),
                self.row('candidate', solver, tick_offset, redundant=int(reduced))]
        if reduced and solver == 'tactix':
            rows.append(self.row('delete', solver, tick_offset, redundant=1))
        rows.append(self.row('source-end', solver, tick_offset, redundant=int(reduced)))
        return self.header() + ''.join(rows)

    def header(self):
        return '# tactix-transitive-trace\t2\n# ' + '\t'.join(FIELDS) + '\n'

    def parse(self, text, solver='tactix', phase=7):
        return read_phase(io.StringIO(text), solver, phase)

    def test_same_physical_reduction_across_log_shapes(self):
        for solver in ('tactix', 'kissat'):
            p = self.parse(self.log(solver), solver)
            self.assertEqual(reduced_pairs(p), {(3, 4): 1})

    def test_empty_tactix_source_can_omit_end(self):
        text = self.header() + self.row('source-begin', candidate_count=0)
        self.assertEqual(len(self.parse(text)['sources']), 1)
        with self.assertRaises(AssertionError):
            self.parse(text.replace('tactix\t', 'kissat\t'), 'kissat')

    def test_budget_limited_source_need_not_exhaust_candidates(self):
        text = self.log().replace('\t1\t2\t', '\t3\t4\t')
        source = self.parse(text)['sources'][0]
        self.assertEqual(source['begin']['candidate_count'], 3)
        self.assertEqual(source['begin']['out_degree'], 4)
        self.assertEqual(len(source['candidates']), 1)

    def test_phase_filter_preserves_global_sequence(self):
        text = self.log() + self.log(tick_offset=10).split('\n', 2)[2].replace('\t0\t7\t', '\t1\t8\t')
        p = self.parse(text, phase=8)
        self.assertEqual(p['total_sources'], 2)
        self.assertEqual(p['sources'][0]['begin']['conflicts'], 8)

    def test_duplicate_copies_remain_a_multiset(self):
        text = self.log() + self.log(tick_offset=10).split('\n', 2)[2].replace('\t0\t7\t', '\t1\t7\t')
        self.assertEqual(reduced_pairs(self.parse(text)), {(3, 4): 2})

    def test_rejects_schema_width_truncation_and_foreign_event(self):
        text = self.log()
        for bad in (text.replace('trace\t2', 'trace\t1'), text[:-1],
                    text.replace('source-begin', 'endpoint'), text + '\n',
                    text.replace('\t-\n', '\n'), text.replace('tactix\t', 'kissat\t')):
            with self.subTest(bad=bad), self.assertRaises((AssertionError, ValueError)):
                self.parse(bad)

    def test_rejects_bad_values_and_effort(self):
        for updates in ({'source': 0}, {'redundant': 2}, {'source_value': 2},
                        {'ticks_after': 9}, {'candidate_count': 3}, {'source_internal': -1}):
            text = self.log().replace(self.row('source-begin'), self.row('source-begin', **updates))
            with self.subTest(updates=updates), self.assertRaises(AssertionError):
                self.parse(text)

    def test_rejects_sequence_identity_ordinal_and_missing_end(self):
        text = self.log()
        bad_rows = [self.row('candidate', redundant=1, candidate_ordinal=1),
                    self.row('candidate', redundant=1, source=9),
                    self.row('candidate', redundant=1, conflicts=6)]
        for row in bad_rows:
            with self.assertRaises(AssertionError):
                self.parse(text.replace(self.row('candidate', redundant=1), row))
        for bad in (text.replace(self.row('source-end', redundant=1), ''),
                    text.replace('\t0\t7\t', '\t2\t7\t')):
            with self.assertRaises(AssertionError):
                self.parse(bad)

    def test_rejects_unjustified_delete_and_missing_phase(self):
        with self.assertRaises(AssertionError):
            self.parse(self.log().replace(self.row('candidate', redundant=1), self.row('candidate')))
        with self.assertRaises(AssertionError):
            self.parse(self.log(), phase=8)

    def test_rejects_backwards_cumulative_effort(self):
        text = self.log().replace(self.row('source-end', redundant=1),
                                  self.row('source-end', redundant=1, ticks_after=13))
        with self.assertRaises(AssertionError):
            self.parse(text)

    def boundaries(self):
        return [dict(name='transitive', conflicts='7', decisions='12', seq=str(i),
                     stage=stage, binary=str(count))
                for i, (stage, count) in enumerate((('begin', 96023), ('end', 95902),
                                                     ('begin', 95938), ('end', 95936)))]

    def test_multiple_invocations_share_one_conflict(self):
        drops = binary_drops(self.boundaries(), 'transitive', 7)
        self.assertEqual([d['drop'] for d in drops], [121, 2])
        self.assertEqual(sum(d['drop'] for d in drops), 123)

    def test_boundary_framing_rejected(self):
        rows = self.boundaries()
        for bad in (rows[1:], rows[:-1], rows[:1]+rows, []):
            with self.assertRaises(AssertionError):
                binary_drops(bad, 'transitive', 7)
        with self.assertRaises(AssertionError):
            binary_drops(rows, 'transitive', 8)


if __name__ == '__main__':
    unittest.main()
