import os
from pathlib import Path
import unittest
from unittest.mock import patch

from solver_environment import with_solver_environment


class SolverEnvironmentTests(unittest.TestCase):
    def test_exact_binary_and_restoration(self):
        with patch.dict(os.environ, {'TACTIX_SLOT_DISABLE': 'old'}, clear=True):
            def measured(command):
                self.assertEqual(os.environ['TACTIX_SLOT_DISABLE'], 'TransitiveReduce')
                return {'exit_code': 20}
            wrapped = with_solver_environment(measured, Path('/pin/solver'),
                                               {'TACTIX_SLOT_DISABLE': 'TransitiveReduce'})
            result = wrapped(['perf', 'stat', '--', Path('/pin/solver'), 'input'])
            self.assertEqual(result['exit_code'], 20)
            self.assertEqual(result['solver_environment'], {'TACTIX_SLOT_DISABLE': 'TransitiveReduce'})
            self.assertEqual(os.environ['TACTIX_SLOT_DISABLE'], 'old')

    def test_absent_value_removed_after_call(self):
        with patch.dict(os.environ, {}, clear=True):
            wrapped = with_solver_environment(lambda c: {}, '/pin/solver',
                                               {'TACTIX_SLOT_DISABLE': 'TransitiveReduce'})
            wrapped(['/pin/solver'])
            self.assertNotIn('TACTIX_SLOT_DISABLE', os.environ)

    def test_failure_restores_environment(self):
        with patch.dict(os.environ, {'KISSAT_DIAGNOSTIC': 'old'}, clear=True):
            def fail(command):
                raise RuntimeError('measured failure')
            wrapped = with_solver_environment(fail, '/pin/solver',
                {'KISSAT_DIAGNOSTIC': 'new', 'TACTIX_SLOT_DISABLE': 'TransitiveReduce'})
            with self.assertRaisesRegex(RuntimeError, 'measured failure'):
                wrapped(['/pin/solver'])
            self.assertEqual(dict(os.environ), {'KISSAT_DIAGNOSTIC': 'old'})

    def test_checker_and_similar_paths_untouched(self):
        with patch.dict(os.environ, {}, clear=True):
            result = {'exit_code': 0}
            def measured(command, marker, *, timeout):
                self.assertNotIn('TACTIX_SLOT_DISABLE', os.environ)
                self.assertEqual((marker, timeout), (4, 5))
                return result
            wrapped = with_solver_environment(measured, '/pin/solver',
                                               {'TACTIX_SLOT_DISABLE': 'TransitiveReduce'})
            for executable in ('/pin/checker', '/pin/solver-extra'):
                self.assertIs(wrapped([executable], 4, timeout=5), result)

    def test_empty_environment_and_input_immutability(self):
        original = {'exit_code': 20}
        variables = {}
        command = ['/pin/solver']
        wrapped = with_solver_environment(lambda c: original, '/pin/solver', variables)
        variables['TACTIX_SLOT_DISABLE'] = 'TransitiveReduce'
        self.assertEqual(wrapped(command), dict(original, solver_environment={}))
        self.assertEqual(command, ['/pin/solver'])
        self.assertEqual(original, {'exit_code': 20})

    def test_reject_non_solver_and_malformed_environment(self):
        for variables in ({'PATH': '/tmp'}, {'TACTIX_bad': 'x'},
                          {'TACTIX_X': 1}, {'TACTIX_X': 'x\0y'}, {1: 'x'}):
            with self.subTest(variables=variables), self.assertRaises(ValueError):
                with_solver_environment(lambda c: {}, '/pin/solver', variables)

    def test_selected_bve_conflict_is_solver_only(self):
        key = 'SAT_BVE_CANDIDATE_CONFLICT'
        with patch.dict(os.environ, {key: '17'}, clear=True):
            seen = []
            def measured(command):
                seen.append(os.environ.get(key))
                return {}
            wrapped = with_solver_environment(measured, '/pin/solver', {key: '5187498'})
            wrapped(['/pin/checker'])
            result = wrapped(['/pin/solver'])
            wrapped(['/pin/checker'])
            self.assertEqual(seen, ['17', '5187498', '17'])
            self.assertEqual(result['solver_environment'], {key: '5187498'})

    def test_selected_bve_conflict_cleanup_on_exception(self):
        key = 'SAT_BVE_CANDIDATE_CONFLICT'
        with patch.dict(os.environ, {}, clear=True):
            def fail(command):
                self.assertEqual(os.environ[key], '0')
                raise RuntimeError('observer failure')
            wrapped = with_solver_environment(fail, '/pin/solver', {key: '0'})
            with self.assertRaises(RuntimeError):
                wrapped(['/pin/solver'])
            self.assertNotIn(key, os.environ)

    def test_selected_bve_conflict_rejects_invalid_values(self):
        for value in ('', '-1', '01', '1.0', ' 1', '1\n', str(2**64), 1):
            with self.subTest(value=value), self.assertRaises(ValueError):
                with_solver_environment(lambda c: {}, '/pin/solver',
                                        {'SAT_BVE_CANDIDATE_CONFLICT': value})
        with_solver_environment(lambda c: {}, '/pin/solver',
                                {'SAT_BVE_CANDIDATE_CONFLICT': str(2**64 - 1)})

    def test_selected_bve_does_not_enable_other_sat_controls(self):
        for key in ('SAT_BVE_CANDIDATE_EXIT_AFTER_END', 'SAT_BVE_CANDIDATE_EXTERNAL',
                    'SAT_EVENT_TRACE_EXIT', 'SAT_BVE_HEAP_CONFLICT'):
            with self.subTest(key=key), self.assertRaises(ValueError):
                with_solver_environment(lambda c: {}, '/pin/solver', {key: '1'})


if __name__ == '__main__':
    unittest.main()
