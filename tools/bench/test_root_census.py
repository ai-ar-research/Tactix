import unittest
from root_census import FIELDS, parse


def line(**changes):
    values = dict.fromkeys(sorted(FIELDS),0)
    values.update(seq=1,stage='bve-after',vars=10,fixed=8,fixed_active=8,unassigned=2,
                  dense_words=4,dense_capacity=16,dense_holes=3,
                  heap_arena=100,heap_live=30,heap_free=70)
    values.update(changes)
    return 'census\t'+'\t'.join(f'{k}={v}' for k,v in values.items())


class RootCensusTest(unittest.TestCase):
    def test_valid_rows(self):
        rows=parse(line()+'\n'+line(seq=2,stage='phase-exit',conflicts=2))
        self.assertEqual(len(rows),2)
        self.assertEqual(rows[0]['heap_free'],70)

    def test_bad_schema(self):
        for text in ('',line()+'\tvars=1',line().replace('vars=10','missing=10'),line(stage='unknown')):
            with self.subTest(text=text),self.assertRaises(ValueError):parse(text)

    def test_bad_sequence_or_work(self):
        for text in (line(seq=2),line()+'\n'+line(),line(conflicts=2)+'\n'+line(seq=2,conflicts=1)):
            with self.assertRaises(ValueError):parse(text)

    def test_bad_populations(self):
        for change in ({'fixed_active':9},{'fixed':11},{'retired':3}):
            with self.assertRaises(ValueError):parse(line(**change))

    def test_bad_sizes(self):
        for change in ({'watch_words':1},{'dense_holes':5},{'dense_capacity':15},
                       {'arena_words':1},{'reconstruction_words':1},{'selected_size_bytes':1}):
            with self.assertRaises(ValueError):parse(line(**change))

    def test_allocator_conservation(self):
        with self.assertRaises(ValueError):parse(line(heap_free=71))

    def test_invalid_numbers(self):
        for value in (-1,'nan','1.0','１２'):
            with self.assertRaises(ValueError):parse(line(vars=value))

    def test_hwm_decrease_retained(self):
        rows=parse(line(max_rss_kib=20)+'\n'+line(seq=2,max_rss_kib=19))
        self.assertEqual(rows[1]['max_rss_kib'],19)


if __name__=='__main__': unittest.main()
