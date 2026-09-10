import unittest

from bump_groups import compare_groups, group_bumps


def rows(variables, conflict=10, decisions=20):
    return [dict(conflicts=conflict, decisions=decisions, variable=v)
            for v in variables]


class BumpGroupTests(unittest.TestCase):
    def test_equal_and_empty(self):
        self.assertEqual(compare_groups([], [])['group_counts'], [0, 0])
        self.assertEqual(compare_groups(rows([1, 2]), rows([1, 2]))['differences'], [])

    def test_order_only(self):
        diff = compare_groups(rows([1, 2]), rows([2, 1]))['differences'][0]
        self.assertTrue(diff['same_members'])
        self.assertEqual(diff['candidate_order'], [1, 2])
        self.assertEqual(diff['reference_only'], [])

    def test_duplicate_membership(self):
        diff = compare_groups(rows([1, 1, 2]), rows([1, 2, 2]))['differences'][0]
        self.assertFalse(diff['same_members'])
        self.assertEqual(diff['candidate_only'], [1])
        self.assertEqual(diff['reference_only'], [2])

    def test_missing_group_and_exact_decision_clock(self):
        result = compare_groups(rows([1]), rows([1], decisions=21))
        self.assertEqual([d['clock'] for d in result['differences']], [[10, 20], [10, 21]])
        self.assertEqual([d['counts'] for d in result['differences']], [[1, 0], [0, 1]])

    def test_counter_regression(self):
        for suffix in (rows([2], conflict=9, decisions=21), rows([2], conflict=11, decisions=19)):
            with self.assertRaises(ValueError):
                group_bumps(rows([1]) + suffix)

    def test_raw_input_not_mutated_or_state_normalized(self):
        left = rows([1]); right = rows([1])
        left[0]['stamp'] = 99; right[0]['stamp'] = 109
        self.assertEqual(compare_groups(left, right)['differences'], [])
        self.assertEqual(left[0]['stamp'], 99)
        self.assertEqual(right[0]['stamp'], 109)


if __name__ == '__main__':
    unittest.main()
