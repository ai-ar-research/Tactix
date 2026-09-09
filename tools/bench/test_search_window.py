import unittest
from search_window import FIELDS, MACRO, read_search, macro_replay


class SearchWindowTests(unittest.TestCase):
    def search(self):
        values = dict.fromkeys(FIELDS, '0')
        values.update(solver='tactix', event='conflict', conflicts='10', mode='focused',
                      digest='0000000000000000', top32='-', payload='1,2')
        return '# tactix-event-trace\t1\n# detail-conflicts\t10\t11\n# ' + '\t'.join(FIELDS) + '\n' + '\t'.join(values[k] for k in FIELDS) + '\n'

    def test_search_complete(self):
        self.assertEqual(read_search(self.search(), solver='tactix', window=(10, 11), cap=2)[0]['event'], 'conflict')

    def test_search_schema_and_state(self):
        text = self.search()
        for bad in (text[:-1], text.replace('conflicts\t10\t11', 'conflicts\t9\t11'),
                    text.replace('tactix\t0\tconflict', 'kissat\t0\tconflict'),
                    text.replace('tactix\t0\tconflict', 'tactix\t1\tconflict'),
                    text.replace('focused', 'unknown'), text.replace('0000000000000000', 'bad')):
            with self.assertRaises(ValueError):
                read_search(bad, solver='tactix', window=(10, 11), cap=2)

    def test_search_empty_and_cap(self):
        for text, cap in (('\n'.join(self.search().splitlines()[:3])+'\n', 2), (self.search(), 1)):
            with self.assertRaises(ValueError):
                read_search(text, solver='tactix', window=(10, 11), cap=cap)

    def macro(self, events):
        header = '# tactix-macro-boundary\t1\n# ' + '\t'.join(MACRO) + '\n'
        rows = []
        for seq, event in enumerate(events):
            r = dict.fromkeys(MACRO, '0'); r.update(solver='tactix', seq=str(seq), event=event, mode='none')
            rows.append('\t'.join(r[k] for k in MACRO))
        return header + '\n'.join(rows) + '\n'

    def test_macro_exact_and_known_extras(self):
        a = self.macro(['startup_begin', 'startup_end'])
        self.assertEqual(macro_replay(a, a)['extra_counts'], {})
        b = self.macro(['trace_begin', 'startup_begin', 'retier', 'checkpoint', 'startup_end'])
        self.assertEqual(macro_replay(a, b)['extra_counts'], dict(trace_begin=1, retier=1, checkpoint=1))

    def test_macro_unknown_and_missing(self):
        a = self.macro(['startup_begin', 'startup_end'])
        for events in (['startup_begin'], ['startup_end', 'startup_begin'], ['startup_begin', 'unknown', 'startup_end']):
            with self.assertRaises(ValueError):
                macro_replay(a, self.macro(events))

    def test_macro_changed_original_not_hidden(self):
        a = self.macro(['startup_begin', 'retier', 'startup_end'])
        b = a.replace('retier\t0', 'retier\t1')
        with self.assertRaises(ValueError):
            macro_replay(a, b)

    def test_macro_framing_and_repeated_start(self):
        a = self.macro(['startup_begin'])
        for bad in (a[:-1], a.replace('tactix\t0', 'tactix\t1'), self.macro(['trace_begin', 'trace_begin', 'startup_begin'])):
            with self.assertRaises(ValueError):
                macro_replay(a, bad)


if __name__ == '__main__':
    unittest.main()
