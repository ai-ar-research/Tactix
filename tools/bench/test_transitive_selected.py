import io
import unittest

import test_transitive_trace
from transitive_selected import read_selected


class SelectedTransitiveTests(unittest.TestCase):
    def setUp(self):
        self.fixture = test_transitive_trace.TransitiveTraceTests()

    def row(self, event, **kwargs):
        return self.fixture.row(event, **kwargs)

    def endpoint(self, **kwargs):
        return self.row('endpoint', destination=0, candidate_count=0, out_degree=0,
                        ticks_before=0, ticks_after=0, **kwargs)

    def log(self):
        return (self.fixture.header() + self.endpoint() +
                self.row('admission', candidate_count=0, out_degree=0, copies=1,
                         ticks_before=0, ticks_after=0, skip_reason='admitted') +
                self.row('source-begin') +
                self.row('walk-node', candidate_count=0, out_degree=0,
                         ticks_before=12, ticks_after=13, skip_reason='node=-3') +
                self.row('candidate', redundant=1) + self.row('delete', redundant=1) +
                self.row('source-end', redundant=1))

    def parse(self, text):
        return read_selected(iter(io.StringIO(text)), 'tactix', (3, 4), 7)

    def test_complete_selected_walk(self):
        self.assertEqual(self.parse(self.log())['total_rows'], 7)

    def test_endpoint_without_source_is_observed_not_missing(self):
        p = self.parse(self.fixture.header()+self.endpoint())
        self.assertEqual(p['source_frame_rows'], 0)
        self.assertEqual(len(p['rows']), 1)

    def test_repeated_endpoint_is_not_an_extra_source(self):
        p = self.parse(self.fixture.header()+2*self.endpoint())
        self.assertEqual(p['source_frame_rows'], 0)

    def test_rejects_absent_truncated_and_foreign_frames(self):
        for text in (self.fixture.header(), self.log()[:-1],
                     self.log().replace('node=-3', 'node=0'),
                     self.log().replace('admitted', 'guess'),
                     self.log().replace('walk-node', 'walk-watch'),
                     self.log().replace('tactix\t', 'kissat\t')):
            with self.subTest(text=text), self.assertRaises(AssertionError):
                self.parse(text)

    def test_rejects_incomplete_walk_and_bad_ordinals(self):
        walk = self.row('walk-node', candidate_count=0, out_degree=0,
                        ticks_before=12, ticks_after=13, skip_reason='node=-3')
        for text in (self.log().replace(self.row('candidate', redundant=1), ''),
                     self.log().replace(walk, walk.replace('\t0\t4\t', '\t1\t4\t'))):
            with self.assertRaises(AssertionError):
                self.parse(text)


if __name__ == '__main__':
    unittest.main()
