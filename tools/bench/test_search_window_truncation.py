"""A native preprocessing cap must never masquerade as a complete window."""
import unittest
from search_window import FIELDS, read_search


class TruncatedSearchWindowTests(unittest.TestCase):
    def test_explicit_truncation_marker_is_rejected_even_below_reader_cap(self):
        row = dict.fromkeys(FIELDS, '0')
        row.update(solver='kissat', event='sweep_schedule', mode='focused',
                   digest='0000000000000000', top32='-', payload='external=1')
        content = ('# tactix-event-trace\t1\n# detail-conflicts\t25098\t25103\n# '
                   + '\t'.join(FIELDS) + '\n'
                   + '\t'.join(row[key] for key in FIELDS) + '\n'
                   + '# truncated at 250000 records\n')
        with self.assertRaises(ValueError):
            read_search(content, solver='kissat', window=(25098, 25103), cap=1000000)


if __name__ == '__main__':
    unittest.main()
