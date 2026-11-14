"""Runtime task acceptance: verdict/exit/proof oracles, pins, scratch release.

Fake solvers and checkers only; nothing here runs a benchmark instance.
UNKNOWN is a dataset label, not an oracle.
"""
import errno
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

# The checker runtime's anonymous scratch, file-size limits and process-group
# reaping are Linux-specific, like the benchmark cluster it serves.
linux_runtime = unittest.skipUnless(sys.platform == 'linux', 'Linux checker runtime')

ROOT = Path(__file__).resolve().parents[1]
BENCH = ROOT / "tools/bench"
sys.path.insert(0, str(BENCH))
import runtime_task as runner  # noqa: E402
from runtime_task import (atomic_json, check_unsat_proof,  # noqa: E402
                          checked_verdict_valid, checker_timeout_for,
                          expected_matches, measured, positive_timeout,
                          proof_stage, ProofPipelineDeadline,
                          provisional_result, scratch_incomplete,
                          scratch_resource_reason, ScratchCapacityError,
                          solver_command, validate_runtime_pin_contract)
from anonymous_scratch import AnonymousScratch  # noqa: E402


class RuntimeTaskTest(unittest.TestCase):
    def run_case(self, arm, verdict="SAT", solver_exit=10, checker_exit=0,
                 checker_text="s VERIFIED\n", tampered=False,
                 omit_modelcheck=False, certificate="drat", extra_manifest=None):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            campaign = root / "benchmarks/runs/test"
            for name in ("pins", "claims", "results", "provisional"):
                (campaign / name).mkdir(parents=True)
            cache = root / "benchmarks/cache"
            cache.mkdir()
            (cache / "test.xz").write_bytes(b"fixture")
            (campaign / "pins/solver").write_bytes(b"pin")
            checker_names = ["checkmodel", "modelcheck.py"]
            checker_names += (["drat-trim"] if certificate == "drat" else
                              ["dsr-trim", "dsr-compress", "srcheck",
                               "drat-trim", "cake_lpr"])
            for name in checker_names:
                (campaign / "pins" / name).write_bytes(b"pin")
            pin_sha256 = {
                "pins/solver": "bad" if tampered else runner.sha256(campaign / "pins/solver"),
                "pins/checkmodel": runner.sha256(campaign / "pins/checkmodel"),
                "pins/modelcheck.py": runner.sha256(campaign / "pins/modelcheck.py"),
            }
            for name in checker_names:
                pin_sha256[f"pins/{name}"] = runner.sha256(
                    campaign / "pins" / name)
            if omit_modelcheck:
                del pin_sha256["pins/modelcheck.py"]
            manifest = dict(arms={arm: dict(path="pins/solver", arguments=[],
                                           sha256=runner.sha256(campaign / "pins/solver"),
                                           certificate=certificate)},
                            input_sha256={"test.xz": runner.sha256(cache / "test.xz")},
                            pin_sha256=pin_sha256,
                            resource_contract={"solve_timeout": 1,
                                               "checker_timeout": 1})
            manifest.update(extra_manifest or {})
            (campaign / "manifest.json").write_text(json.dumps(manifest))
            (campaign / "matrix.tsv").write_text(
                "task\trank\tarm\ttier\tinstance\texpected\n"
                f"0\t1\t{arm}\tD\ttest.xz\t{verdict}\n")
            calls = []

            def decompress(command, **kwargs):
                self.assertEqual(command[:2], ['xz', '-dc'])
                kwargs['stdout'].write(b'p cnf 1 2\n1 0\n-1 0\n' if verdict == 'UNSAT'
                                       else b'p cnf 1 1\n1 0\n')

            def measured(command, stdout, stderr, metrics, timeout, environment=None):
                calls.append(command)
                stderr.write_text("")
                if len(calls) == 1:
                    stdout.write_text(("c proof-route dsr-trestle\n"
                                       if verdict == "UNSAT" and certificate == "dsr" else "") +
                                      "s " + ("SATISFIABLE" if verdict == "SAT" else "UNSATISFIABLE") + "\n")
                    # Binary DSR with a repeated pivot is a witnessed SR line;
                    # the solver declares the Trestle route for this proof.
                    Path(command[-1]).write_bytes(
                        bytes([1, 2, 2, 0]) if certificate == "dsr" else b"proof")
                    code = solver_exit
                else:
                    name = Path(command[0]).name
                    if certificate == "dsr" and name == "dsr-trim":
                        stdout.write_text("s VERIFIED UNSAT\n")
                        Path(command[-1]).write_bytes(b"binary lsr")
                    elif certificate == "dsr" and name == "dsr-compress":
                        stdout.write_text("")
                        Path(command[-1]).write_bytes(b"text lsr")
                    elif certificate == "dsr" and name == "srcheck":
                        stdout.write_text("s VERIFIED UNSAT\n")
                    else:
                        stdout.write_text(checker_text)
                    code = checker_exit
                return dict(exit_code=code, wall_seconds=1, peak_rss_kb=1, timed_out=False)

            with patch.object(runner, "ROOT", root), patch.object(runner, "CAMPAIGN", campaign), \
                 patch.object(sys, "argv", ["runtime_task.py", str(campaign), "0"]), \
                 patch.dict(os.environ, {"SLURM_JOB_ID": "test", "SLURM_TMPDIR": temporary}), \
                 patch.object(runner.os, "sched_getaffinity", return_value={0},
                              create=True), \
                 patch.object(runner.subprocess, "run", side_effect=decompress), \
                 patch.object(runner, "measured", measured):
                code = runner.main()
            result = json.loads((campaign / "results/000.json").read_text())
            self.assertTrue(result["artifact_deleted"])
            return code, result, calls

    def test_both_arms_are_checked(self):
        for arm in ("tactix", "kissat"):
            code, result, calls = self.run_case(arm)
            self.assertEqual(code, 0)
            self.assertTrue(result["valid"])
            self.assertEqual(len(calls), 2)
            self.assertEqual(Path(calls[1][0]).name, "checkmodel")

    def test_bad_model_or_exit_is_not_valid(self):
        for arm in ("tactix", "kissat"):
            for kwargs in (dict(checker_exit=1), dict(solver_exit=0)):
                code, result, _ = self.run_case(arm, **kwargs)
                self.assertEqual(code, 1)
                self.assertFalse(result["valid"])

    def test_unsat_requires_verification_marker_and_exit(self):
        for arm in ("tactix", "kissat"):
            for text, check_exit, valid in (("s VERIFIED\n", 0, True),
                                           ("s NOT VERIFIED\n", 0, False),
                                           ("s VERIFIED\n", 1, False), ("", 0, False)):
                code, result, calls = self.run_case(arm, "UNSAT", 20, check_exit, text)
                self.assertEqual(result["valid"], valid)
                self.assertEqual(code, 0 if valid else 1)
                self.assertEqual(Path(calls[1][0]).name, "drat-trim")

    def test_dsr_uses_the_reference_pipeline_and_parameters(self):
        code, result, calls = self.run_case(
            "tactix", "UNSAT", 20, certificate="dsr")
        self.assertEqual(code, 0)
        self.assertTrue(result["valid"])
        self.assertEqual(
            [Path(call[0]).name for call in calls[1:]],
            ["dsr-trim", "dsr-compress", "srcheck"])
        # Exactly the Satsuma+Kissat reference invocation: dsr-trim takes no
        # options (backward checking), dsr-compress -d expands its binary LSR,
        # and Trestle reads that text LSR.
        self.assertEqual(len(calls[1]), 4)
        self.assertFalse([arg for arg in map(str, calls[1][1:])
                          if arg.startswith("-")])
        self.assertEqual(str(calls[2][1]), "-d")
        self.assertEqual(str(calls[2][2]), str(calls[1][-1]))
        self.assertEqual(len(calls[3]), 3)
        self.assertEqual(str(calls[3][-1]), str(calls[2][-1]))
        self.assertEqual(result["proof_check"]["pipeline"],
                         "dsr-trim-dsr-compress-trestle")
        self.assertEqual([stage["result"] for stage in
                          result["proof_check"]["stages"]],
                         ["verified", "verified", "verified"])

    def test_changed_pin_stops_before_solver(self):
        for arm in ("tactix", "kissat"):
            code, result, calls = self.run_case(arm, tampered=True)
            self.assertEqual(code, 1)
            self.assertEqual(result["verdict"], "INFRA_ERROR")
            self.assertEqual(calls, [])

    def test_missing_modelcheck_dependency_stops_before_solver(self):
        code, result, calls = self.run_case("tactix", omit_modelcheck=True)
        self.assertEqual(code, 1)
        self.assertEqual(result["verdict"], "INFRA_ERROR")
        self.assertIn("modelcheck.py", result["error"])
        self.assertEqual(calls, [])

    def test_pb_spelling_on_anonymous_scratch(self):
        checker_root = ROOT / 'tools/checkers/bin'
        checker_sources = {
            name: checker_root / name for name in ('veripb', 'cake-pb-cnf')
        }
        missing = [str(path) for path in checker_sources.values()
                   if not path.is_file()]
        if missing:
            self.skipTest('PB checker installation is unavailable: ' +
                          ', '.join(missing))
        with tempfile.TemporaryDirectory() as temporary:
            campaign = Path(temporary) / 'campaign'
            (campaign / 'pins').mkdir(parents=True)
            for name, source in checker_sources.items():
                (campaign / 'pins' / name).symlink_to(source.resolve())
            scratch = AnonymousScratch('/tmp')
            try:
                cnf, proof = scratch / 'original.cnf', scratch / 'proof.pbp'
                original = 'c leading\np cnf 1 2\n1\n0 -1 0\n'
                cnf.write_text(original)
                proof.write_text('pseudo-Boolean proof version 3.0\nrup >= 1 ;\n'
                                 'output NONE ;\nconclusion UNSAT : -1 ;\n'
                                 'end pseudo-Boolean proof ;\n')
                for certificate in ('pbp', 'auto'):
                    (scratch / 'proof.kernel.pbp').write_text(
                        'stale kernel must not survive')
                    result = runner.check_unsat_proof(
                        campaign, certificate, cnf, proof, scratch, 60, 'pbp')
                    self.assertEqual(result['result'], 'verified', result)
                    self.assertEqual(result['checker_input']['original_sha256'],
                                     runner.sha256(cnf))
                    self.assertEqual(cnf.read_text(), original)
                    self.assertEqual((scratch / 'checker.cnf').read_text(),
                                     'p cnf 1 2\n1 0\n-1 0\n')
                    self.assertEqual(
                        [stage['result'] for stage in result['stages']],
                        ['verified', 'verified'])
                cnf.write_text('p cnf 1 1\n1 0\n')
                for certificate in ('pbp', 'auto'):
                    self.assertEqual(runner.check_unsat_proof(
                        campaign, certificate, cnf, proof, scratch, 60,
                        'pbp')['result'], 'not-verified')
            finally:
                scratch.close()
            self.assertFalse(scratch.exists())

    def test_scratch_resource_errors_are_classified(self):
        self.assertEqual(scratch_resource_reason(
            OSError(errno.ENOSPC, 'full')), 'scratch_enospc')
        self.assertEqual(scratch_resource_reason(
            OSError(errno.EFBIG, 'large')), 'scratch_file_limit')
        self.assertEqual(scratch_resource_reason(
            subprocess.CalledProcessError(-signal.SIGXFSZ, ['xz'])),
            'scratch_file_limit')
        self.assertEqual(scratch_resource_reason(
            subprocess.CalledProcessError(128 + signal.SIGXFSZ, ['xz'])),
            'scratch_file_limit')
        self.assertIsNone(scratch_resource_reason(OSError(errno.EIO, 'io')))
        self.assertEqual(scratch_incomplete('scratch_enospc'), {
            'status': 'incomplete', 'reason': 'scratch_enospc', 'details': {}})

    def test_retired_manifest_contracts_fail_before_solving(self):
        for key in runner.RETIRED_CONTRACTS:
            with self.subTest(key=key):
                code, result, calls = self.run_case('tactix', extra_manifest={key: {}})
                self.assertEqual(code, 1)
                self.assertEqual(result['verdict'], 'INFRA_ERROR')
                self.assertIn('retired manifest contract', result['error'])
                self.assertEqual(calls, [])

    @linux_runtime
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
        environment = dict(os.environ, PYTHONPATH=str(BENCH))
        completed = subprocess.run(
            [sys.executable, '-c', script], env=environment,
            capture_output=True, text=True, timeout=10)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(json.loads(completed.stdout)['resource_limit'],
                         'file_size')

    @linux_runtime
    def test_measured_file_limit_detection_never_reads_whole_stderr(self):
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

    @linux_runtime
    def test_native_sigxfsz_is_classified_from_exit_status(self):
        script = r'''import json,resource,tempfile
from pathlib import Path
from runtime_task import measured
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp)
 resource.setrlimit(resource.RLIMIT_FSIZE,(4096,4096))
 result=measured(['head','-c','8192','/dev/zero'],root/'out',root/'err',root/'time',10)
 print(json.dumps(result))
'''
        environment = dict(os.environ, PYTHONPATH=str(BENCH))
        completed = subprocess.run(
            [sys.executable, '-c', script], env=environment,
            capture_output=True, text=True, timeout=10)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        result = json.loads(completed.stdout)
        self.assertEqual(result['resource_limit'], 'file_size')
        self.assertIn(result['exit_code'], (153, -signal.SIGXFSZ))

    def test_recorded_route_rejection_is_not_hidden_by_deadline_during_return(self):
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

    @linux_runtime
    def test_whole_deadline_kills_a_term_ignoring_checker_group_without_grace(self):
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

    @linux_runtime
    def test_anonymous_stage_release_preserves_diagnostics_and_output_size(self):
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

    @linux_runtime
    def test_anonymous_witness_free_route_releases_at_checked_handoffs(self):
        with tempfile.TemporaryDirectory() as directory:
            scratch = AnonymousScratch(directory)
            self.addCleanup(scratch.close)
            cnf, proof = scratch/'input.cnf', scratch/'proof.out'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof.write_bytes(b'original-certificate')
            def canonical(source, destination):
                self.assertTrue(source.exists())
                destination.write_bytes(source.read_bytes())
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
                    self.assertFalse(scratch.contains('proof.out'))
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
                    cnf_scratch_name='input.cnf', proof_scratch_name='proof.out')
            self.assertEqual(result['result'], 'verified')
            self.assertFalse(scratch.contains('checker.cnf'))
            self.assertLessEqual(scratch.high_water_handles, 6)
            self.assertFalse(scratch.exists())

    @linux_runtime
    def test_anonymous_witnessed_route_releases_each_finished_generation(self):
        with tempfile.TemporaryDirectory() as directory:
            scratch = AnonymousScratch(directory)
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

    @linux_runtime
    def test_witnessed_resource_stop_keeps_completed_checker_stages(self):
        with tempfile.TemporaryDirectory() as directory:
            scratch = AnonymousScratch(directory)
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
                    raise ScratchCapacityError(
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
                with self.assertRaises(ScratchCapacityError) as caught:
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

    @linux_runtime
    def test_ascii_witnessed_route_never_allocates_binary_intermediate(self):
        with tempfile.TemporaryDirectory() as directory:
            scratch = AnonymousScratch(directory)
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

    @linux_runtime
    def test_anonymous_pbp_releases_proof_only_after_veripb_handoff(self):
        with tempfile.TemporaryDirectory() as directory:
            scratch = AnonymousScratch(directory)
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


class ExperimentalCheckerProfileTest(unittest.TestCase):
    def test_explicit_long_profiles_preserve_independent_checker_and_defaults(self):
        import experimental_tierd as builder
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            reference, frozen, long, fixed = (
                root / name for name in ('ref', 'frozen', 'long', 'fixed'))
            for directory in (reference, frozen, long, fixed):
                directory.mkdir()
            baseline = {'checkers': {}, 'provenance': {}}
            for name in ('checkmodel', 'modelcheck.py', 'drat-trim', 'cake-lpr',
                         'veripb', 'cake-pb-cnf'):
                path = reference / name
                path.write_text(name)
                baseline['checkers'][name] = name
                baseline['provenance'][name] = {'sha256': builder.sha(path)}
            frozen_hashes, long_hashes, fixed_hashes = {}, {}, {}
            for name in ('dsr-trim', 'dsr-compress', 'srcheck'):
                path = frozen / name
                path.write_text('frozen-' + name)
                frozen_hashes[name] = builder.sha(path)
            for name in ('dsr-trim', 'dsr-compress'):
                path = long / ('compress' if name == 'dsr-compress' else name)
                path.write_text('long-' + name)
                long_hashes[name] = builder.sha(path)
                path = fixed / name
                path.write_text('fixed-' + name)
                fixed_hashes[name] = builder.sha(path)
            with patch.multiple(builder, FROZEN_DSR=frozen, LONG_DSR=long,
                                FIXED_LONG_DSR=fixed,
                                FROZEN_DSR_HASHES=frozen_hashes,
                                LONG_DSR_HASHES=long_hashes,
                                FIXED_LONG_DSR_HASHES=fixed_hashes):
                original = builder.checker_sources(reference, baseline, 'dsr')
                selected = builder.checker_sources(reference, baseline, 'dsr', 'long64')
                repaired = builder.checker_sources(
                    reference, baseline, 'dsr', 'long64-fixed')
                self.assertEqual(original['dsr-trim'], frozen / 'dsr-trim')
                self.assertEqual(selected['dsr-trim'], long / 'dsr-trim')
                self.assertEqual(selected['dsr-compress'], long / 'compress')
                self.assertEqual(repaired['dsr-trim'], fixed / 'dsr-trim')
                self.assertEqual(repaired['dsr-compress'], fixed / 'dsr-compress')
                for name in set(original) - set(long_hashes):
                    self.assertEqual(selected[name], original[name])
                    self.assertEqual(repaired[name], original[name])
                for name in ('dsr-trim', 'compress'):
                    path = long / name
                    saved = path.read_bytes()
                    path.write_bytes(b'tampered')
                    with self.assertRaisesRegex(ValueError, 'pin mismatch'):
                        builder.checker_sources(reference, baseline, 'dsr', 'long64')
                    path.write_bytes(saved)
                (long / 'compress').unlink()
                with self.assertRaisesRegex(ValueError, 'pin mismatch'):
                    builder.checker_sources(reference, baseline, 'dsr', 'long64')
                for name in ('dsr-trim', 'dsr-compress'):
                    path = fixed / name
                    saved = path.read_bytes()
                    path.write_bytes(b'tampered')
                    with self.assertRaisesRegex(ValueError, 'pin mismatch'):
                        builder.checker_sources(
                            reference, baseline, 'dsr', 'long64-fixed')
                    path.write_bytes(saved)
                self.assertEqual(builder.checker_sources(reference, baseline, 'dsr'), original)
                with self.assertRaisesRegex(ValueError, 'unsupported DSR checker profile'):
                    builder.checker_sources(reference, baseline, 'dsr', 'unqualified')

    def test_fixed_long_profile_provenance_is_hash_bound(self):
        import experimental_tierd as builder
        current = builder.FIXED_LONG_DSR_PROVENANCE
        self.assertEqual(current['profile'], 'long64-fixed')
        self.assertEqual(current['qualification_job_id'], '1755133')
        self.assertEqual(
            set(current['files']), set(builder.FIXED_LONG_DSR_PROVENANCE['files']))
        patch_rel = 'tools/checkers/patches/dsr-trim-0b8cc0e-forward-elaboration.patch'
        self.assertEqual(current['files'][patch_rel], current['repository_patch_sha256'])
        self.assertEqual(builder.sha(builder.ROOT / patch_rel), current['repository_patch_sha256'])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            artifact = root / 'source.tar'
            artifact.write_bytes(b'source')
            authority = {
                'schema': 'test-profile',
                'profile': 'long64-fixed',
                'files': {'source.tar': builder.sha(artifact)},
            }
            with patch.multiple(builder, ROOT=root,
                                FIXED_LONG_DSR_PROVENANCE=authority):
                selected = builder.dsr_checker_provenance('long64-fixed')
                self.assertEqual(selected, authority)
                selected['files'].clear()
                self.assertTrue(authority['files'])
                artifact.write_bytes(b'tampered')
                with self.assertRaisesRegex(ValueError, 'provenance mismatch'):
                    builder.dsr_checker_provenance('long64-fixed')
            unbound = dict(authority, repository_patch_sha256='0' * 64)
            artifact.write_bytes(b'source')
            with patch.multiple(builder, ROOT=root,
                                FIXED_LONG_DSR_PROVENANCE=unbound):
                with self.assertRaisesRegex(ValueError, 'not file-bound'):
                    builder.dsr_checker_provenance('long64-fixed')
            self.assertIsNone(builder.dsr_checker_provenance('frozen'))
            self.assertIsNone(builder.dsr_checker_provenance('long64'))


if __name__ == "__main__":
    unittest.main()
