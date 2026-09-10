import unittest

from perf_self_pc import bind_self_pc


class SelfPcTests(unittest.TestCase):
    def setUp(self):
        self.inventory = {'sort<T>(iterator, comparator) [clone]':
            dict(address=100, size=10, instruction_addresses={100, 102, 105})}
        self.selected = lambda name: name.startswith('sort<')

    def test_demangler_alias_preserves_pc_identity(self):
        name = 'sort<T>(iterator, iterator, long, comparator) [clone]'
        self.assertEqual(bind_self_pc(102, name, self.inventory, self.selected),
                         'sort<T>(iterator, comparator) [clone]')

    def test_unselected_leaf_is_not_attributed(self):
        self.assertIsNone(bind_self_pc(102, 'other', self.inventory, self.selected))

    def test_selected_alias_cannot_hide_missing_or_interior_pc(self):
        for pc in (99, 101, 110, 1000):
            with self.subTest(pc=pc), self.assertRaises(ValueError):
                bind_self_pc(pc, 'sort<T>(alias)', self.inventory, self.selected)

    def test_overlapping_intervals_are_rejected(self):
        self.inventory['sort<U>(alias)'] = dict(address=101, size=8, instruction_addresses={102})
        with self.assertRaises(ValueError):
            bind_self_pc(102, 'sort<T>(alias)', self.inventory, self.selected)

    def test_unselected_interval_cannot_claim_selected_sample(self):
        self.inventory = {'other': self.inventory.pop(next(iter(self.inventory)))}
        with self.assertRaises(ValueError):
            bind_self_pc(102, 'sort<T>(alias)', self.inventory, self.selected)

    def test_invalid_addresses_and_sizes(self):
        for pc in (True, -1, 100.0):
            with self.subTest(pc=pc), self.assertRaises(ValueError):
                bind_self_pc(pc, 'sort<T>(alias)', self.inventory, self.selected)
        for size in (0, -1, True, 10.0):
            with self.subTest(size=size), self.assertRaises(ValueError):
                self.inventory[next(iter(self.inventory))]['size'] = size
                bind_self_pc(102, 'sort<T>(alias)', self.inventory, self.selected)


if __name__ == '__main__':
    unittest.main()
