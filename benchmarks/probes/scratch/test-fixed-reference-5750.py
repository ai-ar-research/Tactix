#!/usr/bin/env python3
"""Fixed, non-solving negative controls for reference ingestion."""
import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('freeze', Path(__file__).with_name('freeze-kissat-reference-5749.py'))
freeze = importlib.util.module_from_spec(spec)
spec.loader.exec_module(freeze)


class ReferenceTests(unittest.TestCase):
    def setUp(self):
        self.row = dict(arm='kissat', instance='fixed.cnf', expected='UNSAT', rank='1', task='1')
        self.record = dict(self.row, expected_match=True,
            solve=dict(timed_out=False, exit_code=20, wall_seconds=1.0))

    def test_pending_is_usable_not_verified(self):
        freeze.validate(self.row, self.record)
        self.assertNotIn('valid', self.record)

    def test_verified_is_usable(self):
        freeze.validate(self.row, dict(self.record, valid=True))

    def test_rejected_certificate(self):
        with self.assertRaises(ValueError):
            freeze.validate(self.row, dict(self.record, valid=False))

    def test_wrong_identity(self):
        for key, value in dict(arm='tactix', instance='other.cnf', expected='SAT', rank=2, task=2).items():
            with self.subTest(key=key), self.assertRaises(ValueError):
                freeze.validate(self.row, dict(self.record, **{key: value}))

    def test_invalid_solve(self):
        for changes in [dict(timed_out=True), dict(exit_code=10), dict(wall_seconds=0),
                        dict(wall_seconds=float('nan')), dict(wall_seconds=float('inf'))]:
            record = copy.deepcopy(self.record)
            record['solve'].update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                freeze.validate(self.row, record)

    def test_wrong_verdict(self):
        with self.assertRaises(ValueError):
            freeze.validate(self.row, dict(self.record, expected_match=False))


if __name__ == '__main__':
    unittest.main()
