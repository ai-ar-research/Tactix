"""Non-solving acceptance tests: UNKNOWN is a dataset label, not an oracle."""
import json
from pathlib import Path
import tempfile
import unittest
from runtime_task import (atomic_json, checked_verdict_valid, checker_timeout_for,
                          expected_matches,
                          positive_timeout, provisional_result, solver_command)


class RuntimeTaskTests(unittest.TestCase):
    def test_drat_stage_sets_inner_timeout_without_extending_outer_window(self):
        from unittest.mock import patch
        from runtime_task import proof_stage
        calls = []
        def measured(command, stdout, stderr, metrics, timeout, environment=None):
            calls.append((command, timeout))
            stdout.write_text('s VERIFIED\n')
            stderr.write_text('')
            return dict(exit_code=0, wall_seconds=0, timed_out=False)
        with tempfile.TemporaryDirectory() as directory, patch('runtime_task.measured', measured):
            command = ['drat-trim', 'input.cnf', 'proof.drat', '-L', 'proof.lrat']
            result = proof_stage(Path(directory), 'drat-trim', command, 57600)
            expected = command[:3] + ['-t', '57600'] + command[3:]
            self.assertEqual(calls, [(expected, 57600)])
            self.assertEqual(result['command'], expected)
            self.assertEqual(result['timeout_seconds'], 57600)
            self.assertEqual(result['result'], 'verified')

    def test_atomic_json_creates_missing_output_directory(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "results" / "007.json"
            atomic_json(path, {"task": 7})
            self.assertEqual(json.loads(path.read_text()), {"task": 7})
            self.assertFalse(path.with_suffix(".json.tmp").exists())

    def sample_result(self):
        return dict(task=0, rank=32, arm='tactix', tier='D', instance='fixed.cnf',
                    expected='UNSAT', verdict='UNSAT', expected_match=True,
                    solve=dict(exit_code=20, timed_out=False), node='test',
                    certificate_bytes=12, certificate_sha256='hash',
                    solver_statistics=['c conflicts 1'],
                    solver_stderr_tail=['[cong-time] stage output'],
                    valid=False, cleanup_pending=True, proof_check=None)

    def test_provisional_publishes_diagnostics_without_changing_work(self):
        original = self.sample_result()
        projected = provisional_result(original, 'fixed timestamp')
        for key in ('solver_stderr_tail', 'solver_statistics', 'solve',
                    'certificate_bytes', 'certificate_sha256', 'expected_match'):
            self.assertEqual(projected[key], original[key])
        self.assertEqual(projected['recorded_at'], 'fixed timestamp')
        self.assertTrue(projected['validation_pending'])
        self.assertNotIn('validation_pending', original)

    def test_provisional_never_inherits_final_acceptance(self):
        original = self.sample_result()
        original.update(valid=True, proof_check={'result': 'verified'}, model_check={'valid': True},
                        artifact_deleted=True, cleanup_pending=False)
        original['solver_stderr_tail'] = ['s VERIFIED']
        projected = provisional_result(original, 'fixed')
        self.assertTrue(projected['validation_pending'])
        for key in ('valid', 'proof_check', 'model_check', 'artifact_deleted', 'cleanup_pending'):
            self.assertNotIn(key, projected)

    def test_empty_diagnostics_and_nonmatching_verdict_are_preserved(self):
        original = self.sample_result()
        original.update(solver_stderr_tail=[], verdict='UNKNOWN', expected_match=False)
        projected = provisional_result(original, 'fixed')
        self.assertEqual(projected['solver_stderr_tail'], [])
        self.assertEqual(projected['verdict'], 'UNKNOWN')
        self.assertFalse(projected['expected_match'])

    def test_known_label_requires_matching_verdict(self):
        for expected in ('SAT', 'UNSAT'):
            for verdict in ('SAT', 'UNSAT', 'UNKNOWN', 'INFRA_ERROR'):
                self.assertEqual(expected_matches(expected, verdict), expected == verdict)

    def test_unknown_label_admits_only_checked_solver_verdicts(self):
        for verdict, code in [('SAT', 10), ('UNSAT', 20)]:
            solve = dict(exit_code=code, timed_out=False)
            self.assertTrue(checked_verdict_valid('UNKNOWN', verdict, solve, True))
            self.assertFalse(checked_verdict_valid('UNKNOWN', verdict, solve, False))

    def test_timeout_error_wrong_exit_or_known_disagreement_is_never_valid(self):
        for expected in ('SAT', 'UNSAT', 'UNKNOWN'):
            for verdict in ('SAT', 'UNSAT', 'UNKNOWN', 'INFRA_ERROR'):
                for code in (None, 0, 1, 10, 20, -9, 124):
                    for timeout in (False, True):
                        for checked in (False, True):
                            want = (checked and not timeout and verdict in ('SAT', 'UNSAT')
                                    and expected in ('UNKNOWN', verdict)
                                    and code == {'SAT': 10, 'UNSAT': 20}.get(verdict))
                            self.assertEqual(checked_verdict_valid(expected, verdict,
                                dict(exit_code=code, timed_out=timeout), checked), want)

    def test_dsr_pin_contract_requires_the_drat_owner_checkers(self):
        import tempfile
        from runtime_task import validate_runtime_pin_contract
        with tempfile.TemporaryDirectory() as d:
            campaign = Path(d); (campaign/'pins').mkdir()
            names = ['checkmodel', 'modelcheck.py', 'tactix', 'dsr-trim', 'dsr-compress', 'srcheck']
            for n in names: (campaign/'pins'/n).write_text('x')
            manifest = {'arms': {'a': {'path': 'pins/tactix', 'certificate': 'dsr'}},
                        'pin_sha256': {'pins/' + n: 'h' for n in names}}
            with self.assertRaises(RuntimeError):
                validate_runtime_pin_contract(campaign, manifest)
            for n in ('drat-trim', 'cake_lpr'):
                (campaign/'pins'/n).write_text('x'); manifest['pin_sha256']['pins/' + n] = 'h'
            validate_runtime_pin_contract(campaign, manifest)

            manifest['arms']['a']['certificate'] = 'auto'
            with self.assertRaises(RuntimeError):
                validate_runtime_pin_contract(campaign, manifest)
            for name in ('veripb', 'cake-pb-cnf'):
                (campaign/'pins'/name).write_text('x')
                manifest['pin_sha256']['pins/' + name] = 'h'
            validate_runtime_pin_contract(campaign, manifest)

    def test_automatic_pb_route_requires_original_cnf_and_complete_pipeline(self):
        from unittest.mock import patch
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            proof = work/'proof.out'
            proof.write_text('pseudo-Boolean proof version 3.0\n')
            cnf = work/'original.cnf'
            cnf.write_text('p cnf 1 2\n1 0\nc comment\n-1 0\n')
            calls = []
            def stage(scratch, name, command, timeout, produced=None):
                calls.append((name, command, produced))
                return dict(name=name, result='verified', exit_code=0,
                            timed_out=False, output_tail='s VERIFIED UNSAT')
            with patch('runtime_task.proof_stage', side_effect=stage):
                result = check_unsat_proof(work, 'auto', cnf, proof, work, 10)
            self.assertEqual(result['result'], 'verified')
            self.assertEqual(result['pipeline'], 'veripb-cake-pb')
            self.assertEqual([c[0] for c in calls], ['veripb', 'cake-pb-cnf'])
            self.assertEqual(calls[0][1][-2:], [cnf, proof])
            self.assertEqual(calls[1][1][1:], [work/'checker.cnf', work/'proof.kernel.pbp'])
            with self.assertRaisesRegex(RuntimeError, 'explicit DSR'):
                check_unsat_proof(work, 'dsr', cnf, proof, work, 10)

    def test_manifest_timeouts_are_enforced(self):
        manifest = {'resource_contract': {'solve_timeout': 300,
                                          'checker_timeout': 16000}}
        self.assertEqual(positive_timeout(manifest, 'solve_timeout'), 300)
        self.assertEqual(positive_timeout(manifest, 'checker_timeout'), 16000)
        self.assertEqual(checker_timeout_for(manifest, 'tierD/x-a.cnf.xz'), 16000)
        manifest['resource_contract']['checker_timeout_overrides'] = {'x-a.cnf.xz': 600}
        self.assertEqual(checker_timeout_for(manifest, 'tierD/x-a.cnf.xz'), 600)
        self.assertEqual(checker_timeout_for(manifest, 'tierD/x-b.cnf.xz'), 16000)
        manifest['resource_contract']['checker_timeout_overrides'] = {'x-a.cnf.xz': 0}
        with self.assertRaises(RuntimeError):
            checker_timeout_for(manifest, 'tierD/x-a.cnf.xz')
        for value in (None, True, False, 0, -1, 1.0, '300'):
            manifest['resource_contract']['solve_timeout'] = value
            with self.assertRaises(RuntimeError):
                positive_timeout(manifest, 'solve_timeout')

    def test_pb_on_anonymous_scratch(self):
        from anonymous_scratch import AnonymousScratch
        from runtime_task import check_unsat_proof
        root = Path(__file__).resolve().parents[2]
        campaign = root/'benchmarks/baselines/sc24-26-32g-5000s'
        scratch = AnonymousScratch()
        try:
            cnf, proof = scratch/'input.cnf', scratch/'proof.out'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof.write_text('pseudo-Boolean proof version 3.0\nrup >= 1 ;\n'
                             'output NONE ;\nconclusion UNSAT : -1 ;\n'
                             'end pseudo-Boolean proof ;\n')
            (scratch/'proof.kernel.pbp').write_text('stale kernel must not survive')
            checked = check_unsat_proof(campaign, 'auto', cnf, proof, scratch, 60)
            self.assertEqual(checked['result'], 'verified', checked)
        finally:
            scratch.close()

    def test_solver_command_supports_validated_trailing_arguments(self):
        arm = {'arguments': ['--before'],
               'trailing_arguments': ['dsr', 'SymmetrySR']}
        self.assertEqual(
            solver_command('solver', arm, 'input.cnf', 'proof.dsr'),
            ['solver', '--before', 'input.cnf', 'proof.dsr', 'dsr',
             'SymmetrySR'])
        for arm in ({'arguments': 'bad'},
                    {'trailing_arguments': [1]},
                    {'arguments': [], 'trailing_arguments': 'bad'}):
            with self.assertRaises(RuntimeError):
                solver_command('solver', arm, 'input.cnf', 'proof.dsr')


if __name__ == '__main__':
    unittest.main()
