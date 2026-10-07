"""Non-solving acceptance tests: UNKNOWN is a dataset label, not an oracle."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import time
import signal
import sys
from unittest.mock import patch
from runtime_task import (atomic_json, atomic_retain, checked_verdict_valid,
                          causal_preflight_contract, checker_timeout_for,
                          gf2_telemetry_contract,
                          expected_matches, failed_proof_retention_contract,
                          positive_timeout, provisional_result, solver_command,
                          terminal_counter_summary)


def capacity_scratch(directory, maximum_live_handles=6):
    import anonymous_scratch
    limit = [anonymous_scratch.resource.RLIM_INFINITY,
             anonymous_scratch.resource.RLIM_INFINITY]
    def get_limit(_kind):
        return tuple(limit)
    def set_limit(_kind, value):
        limit[:] = value
    with patch.object(anonymous_scratch.resource, 'getrlimit', side_effect=get_limit), \
         patch.object(anonymous_scratch.resource, 'setrlimit', side_effect=set_limit):
        return anonymous_scratch.AnonymousScratch(
            directory, minimum_free_bytes=1, maximum_file_bytes=1 << 30,
            maximum_live_handles=maximum_live_handles)


class RuntimeTaskTests(unittest.TestCase):
    def test_opt_in_scratch_capacity_contract_is_exact_and_isolated(self):
        from runtime_task import scratch_capacity_contract
        contract = {
            'schema': 'tactix-anonymous-scratch-capacity-v1',
            'directory': '/tmp', 'minimum_free_bytes': 400_000_000_000,
            'maximum_file_bytes': 60_000_000_000,
            'maximum_live_handles': 6, 'reserve_bytes': 40_000_000_000,
        }
        manifest = {'scratch_policy': 'anonymous-local-files-v1',
                    'resource_contract': {'exclusive_node': True},
                    'anonymous_scratch_capacity': contract}
        self.assertEqual(scratch_capacity_contract(manifest), contract)
        self.assertIsNone(scratch_capacity_contract({}))
        for changed, message in (
                ({**manifest, 'resource_contract': {}}, 'exclusive-node'),
                ({**manifest, 'gf2_telemetry': {}}, 'telemetry'),
                ({**manifest, 'failed_proof_retention': {}}, 'retention'),
                ({**manifest, 'anonymous_scratch_capacity': {
                    **contract, 'minimum_free_bytes': 399_999_999_999}},
                 'reserve')):
            with self.subTest(message=message), self.assertRaisesRegex(
                    RuntimeError, message):
                scratch_capacity_contract(changed)

    def test_measured_file_limit_reaps_the_process_group(self):
        script = r'''import json,os,resource,sys,tempfile,time
from pathlib import Path
from runtime_task import measured
with tempfile.TemporaryDirectory() as tmp:
 resource.setrlimit(resource.RLIMIT_FSIZE,(4096,4096))
 root=Path(tmp);pidfile=root/'child.pid';target=root/'artifact'
 child='import os,subprocess,sys; sleeper=subprocess.Popen([sys.executable,"-c","import time;time.sleep(60)"]);open(sys.argv[1],"w").write(str(sleeper.pid));fd=os.open(sys.argv[2],os.O_CREAT|os.O_WRONLY,0o600);os.write(fd,b"x"*4096);os.write(fd,b"x")'
 result=measured([sys.executable,'-c',child,pidfile,target],root/'out',root/'err',root/'time',10)
 assert result['resource_limit']=='file_size',result
 pid=int(pidfile.read_text());status=Path('/proc')/str(pid)/'stat';until=time.monotonic()+1
 while status.exists():
  try: state=status.read_text().split()[2]
  except OSError: break
  if state=='Z': break
  assert time.monotonic()<until,state
  time.sleep(.01)
 print(json.dumps(result))
'''
        environment = dict(os.environ, PYTHONPATH=str(Path(__file__).parent))
        completed = subprocess.run(
            [sys.executable, '-c', script], env=environment,
            capture_output=True, text=True, timeout=10)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(json.loads(completed.stdout)['resource_limit'],
                         'file_size')

    def test_measured_file_limit_detection_never_reads_whole_stderr(self):
        from runtime_task import measured
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stdout, stderr, metrics = root/'out', root/'err', root/'time'
            original = Path.read_text
            def guarded_read_text(path, *args, **kwargs):
                if path == stderr:
                    raise AssertionError('stderr must be read with a bounded seek')
                return original(path, *args, **kwargs)
            with patch.object(Path, 'read_text', guarded_read_text):
                result = measured(
                    [sys.executable, '-c',
                     'import sys;sys.stderr.write("File too large\\n")'],
                    stdout, stderr, metrics, 10)
            self.assertEqual(result['resource_limit'], 'file_size')

    def test_runtime_capacity_failures_are_terminal_infrastructure_records(self):
        import lzma
        from runtime_task import sha256
        runtime = Path(__file__).with_name('runtime_task.py')
        anonymous = Path(__file__).with_name('anonymous_scratch.py')
        for mode in ('solver', 'admission', 'checkmodel'):
            with self.subTest(mode=mode), \
                 tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                campaign = root/'benchmarks/runs/capacity'
                cache = root/'benchmarks/cache'
                for name in ('pins', 'claims', 'provisional', 'results'):
                    (campaign/name).mkdir(parents=True)
                cache.mkdir(parents=True)
                instance = 'tiny.cnf.xz'
                (cache/instance).write_bytes(lzma.compress(
                    b'p cnf 1 1\n1 0\n'))
                sentinel = root/'solver-started'
                solver = campaign/'pins/solver'
                if mode == 'checkmodel':
                    solver.write_text(
                        '#!/usr/bin/env python3\n'
                        f'open({str(sentinel)!r},"w").write("yes")\n'
                        'print("s SATISFIABLE")\nprint("v 1 0")\n')
                else:
                    solver.write_text(
                        '#!/usr/bin/env python3\nimport os,sys\n'
                        f'open({str(sentinel)!r},"w").write("yes")\n'
                        'fd=os.open(sys.argv[-1],os.O_WRONLY)\n'
                        'os.write(fd,b"x"*4096)\nos.write(fd,b"x")\n')
                solver.chmod(0o550)
                for name in ('modelcheck.py', 'drat-trim'):
                    (campaign/'pins'/name).write_text('pin')
                checkmodel = campaign/'pins/checkmodel'
                checkmodel.write_text(
                    '#!/usr/bin/env python3\nimport os,sys\n'
                    'os.write(1,b"x"*4096)\nos.write(1,b"x")\n')
                checkmodel.chmod(0o550)
                (campaign/'pins/anonymous_scratch.py').write_bytes(
                    anonymous.read_bytes())
                pins = {str(path.relative_to(campaign)): sha256(path)
                        for path in (campaign/'pins').iterdir()}
                minimum = 10**30 if mode == 'admission' else 6*4096 + 4096
                manifest = {
                    'arms': {'tactix': {'path': 'pins/solver',
                              'sha256': sha256(solver),
                              'arguments': [], 'certificate': 'drat'}},
                    'input_sha256': {instance: sha256(cache/instance)},
                    'pin_sha256': pins,
                    'scratch_policy': 'anonymous-local-files-v1',
                    'anonymous_scratch_capacity': {
                        'schema': 'tactix-anonymous-scratch-capacity-v1',
                        'directory': '/tmp',
                        'minimum_free_bytes': minimum,
                        'maximum_file_bytes': 4096,
                        'maximum_live_handles': 6,
                        'reserve_bytes': 4096,
                    },
                    'resource_contract': {'solve_timeout': 10,
                                          'checker_timeout': 10,
                                          'exclusive_node': True},
                }
                (campaign/'manifest.json').write_text(json.dumps(manifest))
                (campaign/'matrix.tsv').write_text(
                    'task\trank\tarm\ttier\tinstance\texpected\n'
                    f'0\t1\ttactix\tD\t{instance}\tUNKNOWN\n')
                environment = dict(os.environ, SLURM_JOB_ID='unit', TMPDIR='/tmp',
                                   PYTHONDONTWRITEBYTECODE='1',
                                   PYTHONPATH=str(Path(__file__).parent))
                completed = subprocess.run(
                    [sys.executable, runtime, campaign, '0'], env=environment,
                    capture_output=True, text=True, timeout=15)
                self.assertEqual(completed.returncode, 1, completed.stderr)
                result = json.loads((campaign/'results/000.json').read_text())
                self.assertEqual(result['verdict'],
                                 'SAT' if mode == 'checkmodel' else 'INFRA_ERROR',
                                 result)
                self.assertFalse(result['valid'])
                self.assertEqual(result['completion'], {
                    'status': 'incomplete',
                    'reason': ('scratch_admission' if mode == 'admission' else
                               'scratch_file_limit'),
                    'details': result['completion']['details'],
                })
                if mode == 'checkmodel':
                    self.assertEqual(result['solve']['exit_code'], 0)
                    self.assertIsNone(result['model_check'])
                self.assertTrue(result['artifact_deleted'])
                self.assertTrue(result['derived_artifacts_deleted'])
                self.assertFalse(result['cleanup_pending'])
                self.assertEqual(sentinel.exists(), mode != 'admission')

    def test_causal_preflight_is_counter_only_and_small(self):
        contract = {
            "schema": "tactix-causal-preflight-v1",
            "maximum_solve_seconds": 60,
            "output": "terminal-controller-counters",
            "retain_full_telemetry": False,
        }
        arm = {"arguments": ["--controller-families=NaryCspPrefix"]}
        manifest = {
            "resource_contract": {"solve_timeout": 60},
            "arms": {"tactix": arm},
            "causal_preflight": contract,
        }
        self.assertEqual(causal_preflight_contract(manifest), contract)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "solver.stdout"
            output.write_text(
                "c controller-rule AffineConsequence requests=9 applications=9 "
                "effective=9 exhausted=0 stale=0 interrupted=0 work=9 overshoot=0\n"
                "c controller-rule NaryCspPrefix requests=0 applications=0 "
                "effective=0 exhausted=0 stale=0 interrupted=0 work=0 overshoot=0\n"
                "c controller-producer NaryCspPrefix calls=224 work=1138621224 "
                "candidates=0 offers=0\n"
                "c controller-producer AffineConsequence calls=999 work=999 "
                "candidates=999 offers=999\n"
                "c controller-producer NaryCspPrefix calls=225 work=1138621225 "
                "candidates=0 offers=0\n"
                "c controller-loop full-restarts=223 partial-restarts=733 "
                "configuration-changes=214 maximum-root-actions=0\n" +
                "c nary-csp-census candidate-domains=3 exact-domains=2 "
                "relation-keys=1 tuples=7 first-decline=no-semantic-candidate "
                "status=declined complete=1 work=123 offers=0\n" +
                "".join(f"service Decide {index}\n" for index in range(1000)) +
                "c decisions   449783 (0 via API)\n"
                "c conflicts   306345\n"
                "c propagations 17669129 (17669129 search, 0 inprocess)\n"
                "c timing      60000 ms solve\n"
                "s UNKNOWN\n")
            summary = terminal_counter_summary(
                output, arm["arguments"], "UNKNOWN")
        self.assertEqual(summary["controller_rules"], {
            "NaryCspPrefix": {
                "requests": 0, "applications": 0, "effective": 0,
                "exhausted": 0, "stale": 0, "interrupted": 0,
                "work": 0, "overshoot": 0,
            }})
        self.assertEqual(summary["controller_producers"]["NaryCspPrefix"], {
            "calls": 225, "work": 1138621225, "candidates": 0, "offers": 0})
        self.assertNotIn("AffineConsequence", summary["controller_rules"])
        self.assertNotIn("AffineConsequence", summary["controller_producers"])
        self.assertEqual(sum("controller-producer" in line
                             for line in summary["lines"]), 1)
        self.assertEqual(sum(line.startswith("c nary-csp-census ")
                             for line in summary["lines"]), 1)
        self.assertIn("candidate-domains=3", summary["lines"][3])
        self.assertIn("first-decline=no-semantic-candidate", summary["lines"][3])
        self.assertFalse(summary["full_telemetry_retained"])
        self.assertLess(len(json.dumps(summary).encode()), 4096)
        self.assertFalse(any(line.startswith("service ") for line in summary["lines"]))

        for changed, message in (
                ({**manifest, "resource_contract": {"solve_timeout": 61}},
                 "exceeds 60"),
                ({**manifest, "gf2_telemetry": {}}, "full GF2 telemetry"),
                ({**manifest, "arms": {"tactix": {"arguments": [
                    "--controller-trace=large.trace"]}}}, "full telemetry artifact")):
            with self.subTest(changed=changed), self.assertRaisesRegex(
                    RuntimeError, message):
                causal_preflight_contract(changed)

    def test_failed_proof_retention_contract_is_bounded_and_relative(self):
        contract = {
            "schema": "tactix-failed-proof-retention-v1",
            "directory": "retained-proofs",
            "maximum_bytes": 1 << 30,
            "purpose": "checker-only recovery; retain only after an unsuccessful UNSAT proof check",
        }
        self.assertEqual(
            failed_proof_retention_contract({"failed_proof_retention": contract}),
            contract)
        self.assertIsNone(failed_proof_retention_contract({}))
        for changed in (
                dict(contract, directory="/tmp/proofs"),
                dict(contract, directory="../proofs"),
                dict(contract, maximum_bytes=0),
                dict(contract, purpose="arbitrary retention"),
                dict(contract, extra=True)):
            with self.subTest(changed=changed), self.assertRaises(RuntimeError):
                failed_proof_retention_contract({"failed_proof_retention": changed})

    def test_v2_telemetry_contract_and_atomic_stored_cap(self):
        contract = {
            "schema": "tactix-experimental-gf2-telemetry-v2",
            "directory": "telemetry", "feedback_compression": "gzip",
            "feedback_wire_version": 2,
            "feedback_stored_max_bytes": 512 << 20,
            "feedback_decoded_max_bytes": 8 << 30,
            "feedback_max_records": 20_000_000,
            "structural_provenance_max_bytes": 256 << 20,
        }
        self.assertEqual(gf2_telemetry_contract({"gf2_telemetry": contract}), contract)
        changed = dict(contract, unknown=True)
        with self.assertRaisesRegex(RuntimeError, "contract fields"):
            gf2_telemetry_contract({"gf2_telemetry": changed})
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory) / "source", Path(directory) / "target"
            source.write_bytes(b"12345")
            atomic_retain(source, target, 5)
            self.assertEqual(target.read_bytes(), b"12345")
            target.unlink()
            with self.assertRaisesRegex(RuntimeError, "cap exceeded"):
                atomic_retain(source, target, 4)
            self.assertFalse(target.exists())

    def test_recorded_route_rejection_is_not_hidden_by_deadline_during_return(self):
        from runtime_task import check_unsat_proof, ProofPipelineDeadline
        for reported in (None, 'dsr-trestle'):
            with self.subTest(reported=reported), tempfile.TemporaryDirectory() as directory:
                work = Path(directory)
                cnf, proof = work/'cnf', work/'proof'
                cnf.write_text('p cnf 1 1\n0\n')
                # Witness-free DSR conflicts with a reported witnessed route;
                # None independently exercises the missing-route rejection.
                proof.write_bytes(b'\x01\x00')
                calls = iter([0.0, 0.0, ProofPipelineDeadline('cut during rejected return'), 0.5])
                def clock():
                    value = next(calls)
                    if isinstance(value, Exception):
                        raise value
                    return value
                with patch('runtime_task.time.monotonic', clock):
                    result = check_unsat_proof(work, 'auto', cnf, proof, work, 1800, reported)
                self.assertFalse(result['timed_out'], 'a recorded rejection must dominate a later deadline')
                self.assertEqual(result['stages'][0]['name'], 'route')
                self.assertEqual(result['stages'][0]['result'], 'rejected')
                self.assertTrue(result['deadline_expired'])

    def test_whole_deadline_includes_python_conversion_and_restores_signal(self):
        from runtime_task import check_unsat_proof
        before = signal.getsignal(signal.SIGALRM)
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            with patch('runtime_task.canonical_checker_cnf',
                       side_effect=lambda *_: time.sleep(1)), \
                 patch('runtime_task.proof_stage') as stage:
                started = time.monotonic()
                result = check_unsat_proof(work, 'drat', work/'cnf', work/'proof',
                                           work, 0.05)
                self.assertLess(time.monotonic() - started, 0.8)
                self.assertTrue(result['timed_out'])
                self.assertEqual(result['stages'][-1]['name'], 'pipeline-deadline')
                stage.assert_not_called()
        self.assertIs(signal.getsignal(signal.SIGALRM), before)
        self.assertEqual(signal.getitimer(signal.ITIMER_REAL)[0], 0)

    def test_whole_deadline_kills_a_term_ignoring_checker_group_without_grace(self):
        from runtime_task import check_unsat_proof, measured
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof, pid = work/'cnf', work/'proof', work/'checker.pid'
            cnf.write_text('p cnf 1 1\n0\n')
            proof.write_text('0\n')
            script = ('import os, signal, time; '
                      'signal.signal(signal.SIGTERM, signal.SIG_IGN); '
                      f'open({str(pid)!r}, "w").write(str(os.getpid())); '
                      'time.sleep(60)')
            def hanging_stage(scratch, _name, _command, timeout, *_args, **_kwargs):
                return measured([sys.executable, '-c', script], work/'stdout',
                                work/'stderr', work/'metrics', timeout)
            with patch('runtime_task.proof_stage', hanging_stage):
                started = time.monotonic()
                result = check_unsat_proof(work, 'drat', cnf, proof, work, 0.2)
                self.assertLess(time.monotonic() - started, 1)
            self.assertTrue(result['timed_out'])
            self.assertTrue(pid.exists(), 'helper must actually reach its hanging body')
            status = Path('/proc') / pid.read_text() / 'stat'
            until = time.monotonic() + 0.2
            while status.exists():
                try:
                    state = status.read_text().split()[2]
                except OSError:
                    break
                if state == 'Z':
                    break
                self.assertLess(time.monotonic(), until, 'checker survived pipeline deadline')
                time.sleep(0.01)

    def test_stage_windows_share_precise_remaining_pipeline_allowance(self):
        from runtime_task import check_unsat_proof
        windows = []
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'cnf', work/'proof'
            cnf.write_text('p cnf 1 1\n0\n')
            proof.write_text('pseudo-Boolean proof version 1.2\n')
            def stage(_scratch, _name, _command, timeout, *_args, **_kwargs):
                windows.append(timeout)
                time.sleep(0.04)
                return dict(result='verified', exit_code=0, timed_out=False, output_tail='')
            with patch('runtime_task.proof_stage', stage):
                result = check_unsat_proof(work, 'auto', cnf, proof, work, 0.4, 'pbp')
            self.assertEqual(result['result'], 'verified')
            self.assertEqual(len(windows), 2)
            self.assertLess(windows[0], 0.4)
            self.assertGreater(windows[0] - windows[1], 0.03)
    def test_malformed_dsr_preserves_proof_failure_classification(self):
        from unittest.mock import patch
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'input.cnf', work/'proof.dsr'
            cnf.write_text('p cnf 1 1\n1 0\n')
            for certificate in ('auto', 'dsr'):
                for data in (b'\x01', b'\x01\x80', b'1', b'--1 0\n'):
                    with self.subTest(certificate=certificate, data=data), patch('runtime_task.measured') as run:
                        proof.write_bytes(data)
                        result = check_unsat_proof(work, certificate, cnf, proof,
                                                   work, 10, 'dsr-cake_lpr')
                        self.assertEqual(result['result'], 'not-verified')
                        self.assertEqual(result['stages'][0]['name'], 'route')
                        self.assertEqual(result['stages'][0]['result'], 'rejected')
                        self.assertFalse(result['timed_out'])
                        run.assert_not_called()

    def test_declared_dsr_route_must_match_finished_certificate(self):
        from unittest.mock import patch
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cnf, proof = work/'input.cnf', work/'proof.dsr'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof.write_bytes(b'\x01\x00')
            for route in (None, 'dsr-trestle', 'unrecognized'):
                with self.subTest(route=route), patch('runtime_task.measured') as run:
                    checked = check_unsat_proof(work, 'dsr', cnf, proof,
                                                work, 10, route)
                    self.assertEqual(checked['result'], 'not-verified')
                    self.assertEqual(checked['stages'][0]['name'], 'route')
                    run.assert_not_called()

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

    def test_anonymous_stage_release_preserves_diagnostics_and_output_size(self):
        from anonymous_scratch import AnonymousScratch
        from runtime_task import proof_stage
        with tempfile.TemporaryDirectory() as directory:
            scratch = AnonymousScratch(directory)
            self.addCleanup(scratch.close)
            produced = scratch/'proof.lrat'
            def measured(_command, stdout, stderr, metrics, _timeout, environment=None):
                stdout.write_text('s VERIFIED\nkept tail\n')
                stderr.write_text('diagnostic\n')
                metrics.write_text('max_rss_kb=7\n')
                produced.write_bytes(b'1234567')
                return dict(exit_code=0, wall_seconds=0.1, timed_out=False,
                            peak_rss_kb=7)
            with patch('runtime_task.measured', measured):
                result = proof_stage(
                    scratch, 'drat-trim', ['checker', 'cnf', 'proof', '-L', produced],
                    10, produced)
            self.assertEqual(result['produced_bytes'], 7)
            self.assertIn('kept tail', result['output_tail'])
            self.assertEqual(result['peak_rss_kb'], 7)
            self.assertFalse(scratch.contains('drat-trim.stdout'))
            self.assertFalse(scratch.contains('drat-trim.stderr'))
            self.assertFalse(scratch.contains('drat-trim.time'))
            self.assertEqual(produced.read_bytes(), b'1234567')

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

    def test_dsr_pin_contract_requires_both_possible_final_checkers(self):
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
            for n in ('cake_lpr', 'drat-trim'):
                (campaign/'pins'/n).write_text('x'); manifest['pin_sha256']['pins/' + n] = 'h'
            validate_runtime_pin_contract(campaign, manifest)

            manifest['arms']['a']['certificate'] = 'auto'
            with self.assertRaises(RuntimeError):
                validate_runtime_pin_contract(campaign, manifest)
            for name in ('veripb', 'cake-pb-cnf'):
                (campaign/'pins'/name).write_text('x')
                manifest['pin_sha256']['pins/' + name] = 'h'
            validate_runtime_pin_contract(campaign, manifest)

    def test_automatic_pb_route_preserves_original_clauses_and_complete_pipeline(self):
        from unittest.mock import patch
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            proof = work/'proof.out'
            proof.write_text('pseudo-Boolean proof version 3.0\n')
            cnf = work/'original.cnf'
            original = '\np cnf 1 2\n1 0\nc comment\n-1 0\n'
            cnf.write_text(original)
            calls = []
            def stage(scratch, name, command, timeout, produced=None):
                calls.append((name, command, produced))
                return dict(name=name, result='verified', exit_code=0,
                            timed_out=False, output_tail='s VERIFIED UNSAT')
            with patch('runtime_task.proof_stage', side_effect=stage):
                result = check_unsat_proof(work, 'auto', cnf, proof, work, 10, 'pbp')
            self.assertEqual(result['result'], 'verified')
            self.assertEqual(result['pipeline'], 'veripb-cake-pb')
            self.assertEqual([c[0] for c in calls], ['veripb', 'cake-pb-cnf'])
            self.assertEqual(calls[0][1][-2:], [work/'checker.cnf', proof])
            self.assertEqual(calls[1][1][1:], [work/'checker.cnf', work/'proof.kernel.pbp'])
            self.assertEqual(cnf.read_text(), original)
            self.assertEqual((work/'checker.cnf').read_text(), 'p cnf 1 2\n1 0\n-1 0\n')
            with self.assertRaisesRegex(RuntimeError, 'explicit DSR'):
                check_unsat_proof(work, 'dsr', cnf, proof, work, 10, 'pbp')

    def test_anonymous_witness_free_route_releases_at_checked_handoffs(self):
        from anonymous_scratch import AnonymousScratch
        from runtime_task import check_unsat_proof
        for retain in (False, True):
            with self.subTest(retain=retain), tempfile.TemporaryDirectory() as directory:
                scratch = (AnonymousScratch(directory) if retain else
                           capacity_scratch(directory))
                self.addCleanup(scratch.close)
                cnf, proof = scratch/'input.cnf', scratch/'proof.out'
                cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
                proof.write_bytes(b'original-certificate')
                checker = None
                def canonical(source, destination):
                    nonlocal checker
                    self.assertTrue(source.exists())
                    destination.write_bytes(source.read_bytes())
                    checker = destination
                    return dict(first_empty_clause=None, duplicate_literal_clauses=0,
                                clauses=2)
                def convert(source, destination):
                    self.assertTrue(source.exists())
                    destination.write_bytes(source.read_bytes())
                    return True
                def stage(_scratch, name, command, _timeout, produced=None, **_kwargs):
                    diagnostics = [f'{name}.{suffix}' for suffix in
                                   ('stdout', 'stderr', 'time')]
                    for diagnostic in diagnostics:
                        scratch/diagnostic
                    if name == 'drat-trim':
                        self.assertFalse(scratch.contains('input.cnf'))
                        self.assertEqual(scratch.contains('proof.out'), retain)
                        self.assertTrue(Path(command[2]).exists())
                        produced.write_bytes(b'elaborated-lrat')
                    else:
                        self.assertEqual(name, 'cake_lpr')
                        self.assertFalse(scratch.contains('proof.drat'))
                        self.assertEqual(Path(command[-1]).read_bytes(), b'elaborated-lrat')
                    for diagnostic in diagnostics:
                        scratch.release(diagnostic)
                    return dict(name=name, result='verified', exit_code=0,
                                timed_out=False, output_tail='verified')
                with patch('runtime_task.canonical_checker_cnf', canonical), \
                     patch('runtime_task.certificate_route', return_value='drat'), \
                     patch('runtime_task.dsr_proof_as_drat', convert), \
                     patch('runtime_task.proof_stage', side_effect=stage):
                    result = check_unsat_proof(
                        Path(directory), 'auto', cnf, proof, scratch, 10,
                        'dsr-cake_lpr', release_intermediates=True,
                        retain_original_proof=retain,
                        cnf_scratch_name='input.cnf', proof_scratch_name='proof.out')
                self.assertEqual(result['result'], 'verified')
                self.assertFalse(scratch.contains('input.cnf'))
                self.assertFalse(scratch.contains('checker.cnf'))
                self.assertEqual(scratch.contains('proof.out'), retain)
                if retain:
                    self.assertEqual(proof.read_bytes(), b'original-certificate')
                    scratch.release('proof.out')
                if not retain:
                    self.assertLessEqual(scratch.high_water_handles, 6)
                self.assertFalse(scratch.exists())

    def test_anonymous_witnessed_route_releases_each_finished_generation(self):
        from anonymous_scratch import AnonymousScratch
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as directory:
            scratch = capacity_scratch(directory)
            self.addCleanup(scratch.close)
            cnf, proof = scratch/'input.cnf', scratch/'proof.out'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof.write_bytes(b'witnessed')
            def stage(_scratch, name, _command, _timeout, produced=None, **_kwargs):
                diagnostics = [f'{name}.{suffix}' for suffix in
                               ('stdout', 'stderr', 'time')]
                for diagnostic in diagnostics:
                    scratch/diagnostic
                if name == 'dsr-trim':
                    self.assertTrue(scratch.contains('proof.out'))
                    self.assertFalse(scratch.contains('input.cnf'))
                    produced.write_bytes(b'binary-lsr')
                elif name == 'dsr-compress':
                    self.assertFalse(scratch.contains('proof.out'))
                    self.assertTrue(scratch.contains('proof.binary.lsr'))
                    produced.write_bytes(b'text-lsr')
                else:
                    self.assertEqual(name, 'srcheck')
                    self.assertFalse(scratch.contains('proof.binary.lsr'))
                    self.assertEqual((scratch/'proof.lsr').read_bytes(), b'text-lsr')
                for diagnostic in diagnostics:
                    scratch.release(diagnostic)
                return dict(name=name, result='verified', exit_code=0,
                            timed_out=False, output_tail='verified')
            def canonical(source, destination):
                destination.write_bytes(source.read_bytes())
                return dict(first_empty_clause=None, duplicate_literal_clauses=0,
                            clauses=2)
            with patch('runtime_task.canonical_checker_cnf', side_effect=canonical), \
                 patch('runtime_task.certificate_route', return_value='dsr'), \
                 patch('runtime_task.dsr_proof_is_binary', return_value=True), \
                 patch('runtime_task.bridge_trivial_input_binary_lsr', return_value=False), \
                 patch('runtime_task.proof_stage', side_effect=stage):
                result = check_unsat_proof(
                    Path(directory), 'auto', cnf, proof, scratch, 10,
                    'dsr-trestle', release_intermediates=True,
                    cnf_scratch_name='input.cnf', proof_scratch_name='proof.out')
            self.assertEqual(result['result'], 'verified')
            self.assertLessEqual(scratch.high_water_handles, 6)
            self.assertFalse(scratch.exists())

    def test_witnessed_resource_stop_keeps_completed_checker_stages(self):
        from anonymous_scratch import ScratchResourceError
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as directory:
            scratch = capacity_scratch(directory)
            self.addCleanup(scratch.close)
            cnf, proof = scratch/'input.cnf', scratch/'proof.out'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof.write_bytes(b'witnessed')
            def canonical(source, destination):
                destination.write_bytes(source.read_bytes())
                return dict(first_empty_clause=None,
                            duplicate_literal_clauses=0, clauses=2)
            def stage(_scratch, name, _command, _timeout, produced=None,
                      **_kwargs):
                diagnostics = [f'{name}.{suffix}' for suffix in
                               ('stdout', 'stderr', 'time')]
                for diagnostic in diagnostics:
                    scratch/diagnostic
                for diagnostic in diagnostics:
                    scratch.release(diagnostic)
                if name == 'dsr-compress':
                    raise ScratchResourceError(
                        'scratch_file_limit', stage=name,
                        measurement=dict(name=name, exit_code=-signal.SIGXFSZ,
                                         timed_out=False,
                                         resource_limit='file_size'))
                produced.write_bytes(b'binary-lsr')
                return dict(name=name, result='verified', exit_code=0,
                            timed_out=False, output_tail='verified')
            with patch('runtime_task.canonical_checker_cnf', side_effect=canonical), \
                 patch('runtime_task.certificate_route', return_value='dsr'), \
                 patch('runtime_task.dsr_proof_is_binary', return_value=True), \
                 patch('runtime_task.bridge_trivial_input_binary_lsr',
                       return_value=False), \
                 patch('runtime_task.proof_stage', side_effect=stage):
                with self.assertRaises(ScratchResourceError) as caught:
                    check_unsat_proof(
                        Path(directory), 'auto', cnf, proof, scratch, 10,
                        'dsr-trestle', release_intermediates=True,
                        cnf_scratch_name='input.cnf',
                        proof_scratch_name='proof.out')
            partial = caught.exception.scratch_details['proof_check']
            self.assertEqual(partial['result'], 'incomplete')
            self.assertEqual(
                [(stage['name'], stage['result']) for stage in partial['stages']],
                [('dsr-trim', 'verified'), ('dsr-compress', 'incomplete')])
            self.assertLessEqual(scratch.high_water_handles, 6)
            self.assertFalse(scratch.exists())

    def test_ascii_witnessed_route_never_allocates_binary_intermediate(self):
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as directory:
            scratch = capacity_scratch(directory)
            self.addCleanup(scratch.close)
            cnf, proof = scratch/'input.cnf', scratch/'proof.out'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof.write_text('1 0 1 0\n')
            def canonical(source, destination):
                destination.write_bytes(source.read_bytes())
                return dict(first_empty_clause=None,
                            duplicate_literal_clauses=0, clauses=2)
            def stage(_scratch, name, _command, _timeout, produced=None,
                      **_kwargs):
                diagnostics = [f'{name}.{suffix}' for suffix in
                               ('stdout', 'stderr', 'time')]
                for diagnostic in diagnostics:
                    scratch/diagnostic
                self.assertFalse(scratch.contains('proof.binary.lsr'))
                if name == 'dsr-trim':
                    self.assertTrue(scratch.contains('proof.out'))
                    produced.write_text('text-lsr')
                else:
                    self.assertEqual(name, 'srcheck')
                    self.assertFalse(scratch.contains('proof.out'))
                    self.assertEqual((scratch/'proof.lsr').read_text(),
                                     'text-lsr')
                for diagnostic in diagnostics:
                    scratch.release(diagnostic)
                return dict(name=name, result='verified', exit_code=0,
                            timed_out=False, output_tail='verified')
            with patch('runtime_task.canonical_checker_cnf', side_effect=canonical), \
                 patch('runtime_task.certificate_route', return_value='dsr'), \
                 patch('runtime_task.dsr_proof_is_binary', return_value=False), \
                 patch('runtime_task.bridge_trivial_input_lsr',
                       return_value=False), \
                 patch('runtime_task.proof_stage', side_effect=stage):
                result = check_unsat_proof(
                    Path(directory), 'auto', cnf, proof, scratch, 10,
                    'dsr-trestle', release_intermediates=True,
                    cnf_scratch_name='input.cnf', proof_scratch_name='proof.out')
            self.assertEqual(result['result'], 'verified')
            self.assertEqual([stage['name'] for stage in result['stages']],
                             ['dsr-trim', 'srcheck'])
            self.assertLessEqual(scratch.high_water_handles, 6)
            self.assertFalse(scratch.exists())

    def test_anonymous_pbp_releases_proof_only_after_veripb_handoff(self):
        from anonymous_scratch import AnonymousScratch
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as directory:
            scratch = capacity_scratch(directory)
            self.addCleanup(scratch.close)
            cnf, proof = scratch/'input.cnf', scratch/'proof.out'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof.write_text('pseudo-Boolean proof version 3.0\n')
            def stage(_scratch, name, _command, _timeout, produced=None, **_kwargs):
                diagnostics = [f'{name}.{suffix}' for suffix in
                               ('stdout', 'stderr', 'time')]
                for diagnostic in diagnostics:
                    scratch/diagnostic
                if name == 'veripb':
                    self.assertTrue(scratch.contains('proof.out'))
                    produced.write_bytes(b'kernel')
                else:
                    self.assertEqual(name, 'cake-pb-cnf')
                    self.assertFalse(scratch.contains('proof.out'))
                    self.assertEqual((scratch/'proof.kernel.pbp').read_bytes(), b'kernel')
                for diagnostic in diagnostics:
                    scratch.release(diagnostic)
                return dict(name=name, result='verified', exit_code=0,
                            timed_out=False, output_tail='verified')
            with patch('runtime_task.certificate_route', return_value='pbp'), \
                 patch('runtime_task.proof_stage', side_effect=stage):
                result = check_unsat_proof(
                    Path(directory), 'auto', cnf, proof, scratch, 10, 'pbp',
                    release_intermediates=True,
                    cnf_scratch_name='input.cnf', proof_scratch_name='proof.out')
            self.assertEqual(result['result'], 'verified')
            self.assertLessEqual(scratch.high_water_handles, 6)
            self.assertFalse(scratch.exists())

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
            checked = check_unsat_proof(campaign, 'auto', cnf, proof, scratch, 60,
                                        'pbp')
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
