import unittest
from reorder_input import read_active, root_boundary, reconstruct


def clause(literals, *, redundant=0, values=None):
    observed = values or {}
    ordered = tuple(sorted(literals))
    return ((redundant, ordered), tuple(literals), (), tuple((lit, observed.get(lit, 0)) for lit in ordered))


class ReorderInputTests(unittest.TestCase):
    def test_unique_root_boundary(self):
        header = '# tactix-macro-boundary\t1\n# solver\tseq\tevent\tconflicts\tdecisions\tpropagations\tlevel\tmode\tunassigned\ttrail\trestarts\treductions\tswitched\trephased\tstamp\n'
        row = 'tactix\t1\treorder_begin\t10\t20\t50\t0\tstable\t3\t4\t0\t0\t0\t0\t4\n'
        self.assertEqual(root_boundary(header + row, event='reorder_begin', conflict=10)['decisions'], '20')
        for bad in (header, header + row + row, header + row.replace('\t0\tstable', '\t1\tstable'),
                    (header + row).rstrip('\n'), header + row.replace('\t20\t', '\t20\textra\t')):
            with self.assertRaises(ValueError):
                root_boundary(bad, event='reorder_begin', conflict=10)

    def test_active_identity_count_and_duplicates(self):
        text = '# tactix-macro-active-set\t1\n# solver\tevent\tconflicts\tactive\n# tactix\treorder_begin\t10\t2\n# external\n3\n1\n'
        self.assertEqual(read_active(text, event='reorder_begin', conflict=10), {1, 3})
        for bad in (text[:-1], text.replace('\t10\t', '\t11\t'), text.replace('\t2\n', '\t3\n'),
                    text.replace('\n3\n1\n', '\n1\n1\n'), text + '7\n', text.replace('\n3\n', '\n0\n'),
                    text.replace('\t10\t2\n', '\t10\textra\t2\n')):
            with self.assertRaises(ValueError):
                read_active(bad, event='reorder_begin', conflict=10)

    def test_binary_multiplicity_and_polarity_combination(self):
        out = reconstruct(1, [clause([1, 2, 3])], [(1, 2), (1, 2), (-1, 3)], {1, 2, 3}, level=0)
        self.assertEqual((out['positive'], out['negative'], out['combined']), (2.5, 1, 4.5))
        self.assertEqual(len(out['binaries']), 3)

    def test_redundant_satisfied_and_root_false(self):
        rows = [clause([1, 2, 3], redundant=1), clause([1, 4, 5], values={4: 1}),
                clause([-1, -4, 6], values={-4: -1})]
        out = reconstruct(1, rows, [(1, 4), (-4, 1)], {1, 2, 3, 5, 6}, level=0)
        self.assertEqual((out['positive'], out['negative'], out['combined']), (0, 1, 1))
        self.assertEqual([x['reason'] for x in out['large']], ['redundant', 'satisfied', 'counted'])
        self.assertFalse(any(x['counted'] for x in out['binaries']))

    def test_cutoff_respects_physical_order(self):
        free = list(range(1, 101))
        after = clause([*free, 101], values={101: 1})
        before = clause([101, *free], values={101: 1})
        a = reconstruct(1, [after], [], set(free), level=0)
        b = reconstruct(1, [before], [], set(free), level=0)
        self.assertEqual(a['positive'], 2.0 ** -98)
        self.assertEqual(b['positive'], 0)

    def test_small_open_count_clamped(self):
        row = clause([1, 2, 3], values={2: -1, 3: -1})
        self.assertEqual(reconstruct(1, [row], [], {1}, level=0)['positive'], 1)

    def test_missing_value_not_assumed_free(self):
        with self.assertRaisesRegex(ValueError, 'unknown binary endpoint'):
            reconstruct(1, [], [(1, 2)], {1}, level=0)

    def test_conflicting_values_and_nonroot_rejected(self):
        for rows, active, level in (([clause([1, 2, 3], values={2: 1})], {1, 2}, 0),
                                    ([clause([1, 2, 3]), clause([1, 2, 4], values={2: 1})], {1}, 0),
                                    ([], {1}, 1), ([], {2}, 0)):
            with self.assertRaises(ValueError):
                reconstruct(1, rows, [], active, level=level)

    def test_nonincident_and_inconsistent_pose_rejected(self):
        bad_pose = ((0, (1, 2, 3)), (1, 2, 4), (), ((1, 0), (2, 0), (3, 0)))
        for rows, binary in (([clause([2, 3, 4])], []), ([bad_pose], []), ([], [(2, 3)]), ([], [(1, 1)])):
            with self.assertRaises(ValueError):
                reconstruct(1, rows, binary, {1, 2, 3, 4}, level=0)


if __name__ == '__main__':
    unittest.main()
