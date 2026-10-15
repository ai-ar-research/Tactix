"""Home-directory scratch, 64 GiB memout rechecks and governor registration.

Fake solvers/checkers are tiny scripts; nothing here runs a real solver. The
recheck preparation test copies the real frozen checker pins into a temporary
campaign (about 200 MB) and reads the real Tier D cache manifest.
"""
import csv
import hashlib
import importlib.util
import json
import lzma
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
import experimental_tierd as et  # noqa: E402
import runtime_task  # noqa: E402
from directory_scratch import DirectoryScratch  # noqa: E402

spec = importlib.util.spec_from_file_location(
    'quota_governor', REPO / 'tools/slurm/quota_governor.py')
qg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qg)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stage(name, code, tail='', timed_out=False, result='error'):
    return dict(name=name, exit_code=code, output_tail=tail, timed_out=timed_out,
                result=result)


def unsat(*stages, valid=False):
    return dict(verdict='UNSAT', valid=valid,
                proof_check=dict(result='verified' if valid else 'not-verified',
                                 stages=list(stages)))


class FailureClasses(unittest.TestCase):
    def test_runtime_records(self):
        cases = [
            (unsat(stage('dsr-trim', 255, 'c Doing BACKWARDS proof checking.\nRan out of memory on xrealloc()')), 'memout'),
            (unsat(stage('drat-trim', 137)), 'memout'),
            (unsat(stage('cake_lpr', 1, 'CakeML heap space exhausted.')), 'memout'),
            (unsat(stage('drat-trim', None, timed_out=True)), 'timeout'),
            (unsat(stage('veripb', 137), stage('pipeline-deadline', None, timed_out=True)), 'timeout'),
            (unsat(stage('dsr-trim', 139)), 'crash'),
            (unsat(stage('srcheck', 1, 's INVALID', result='rejected')), 'rejected'),
            (unsat(stage('route', None, 'DSR proof contains an SR witness', result='rejected')), 'rejected'),
            (unsat(stage('srcheck', 2, 'odd')), 'other'),
            (unsat(stage('cake_lpr', 0, 's VERIFIED UNSAT', result='verified'), valid=True), None),
            (dict(verdict='UNSAT', valid=False, proof_check=None), 'other'),
            (dict(verdict='SAT', valid=False, model_check=dict(
                exit_code=137, timed_out=False, stdout_tail=[], stderr_tail=[])), 'memout'),
            (dict(verdict='UNKNOWN', valid=False), None),
        ]
        for record, expected in cases:
            self.assertEqual(et.result_failure_class(record, None), expected, record)
        self.assertEqual(et.check_failure_class(None), None)

    def test_unfinished_needs_a_provisional_verdict(self):
        self.assertEqual(et.result_failure_class(None, dict(verdict='UNSAT')), 'unfinished')
        self.assertEqual(et.result_failure_class(dict(verdict='INFRA_ERROR'),
                                                 dict(verdict='SAT')), 'unfinished')
        self.assertIsNone(et.result_failure_class(None, dict(verdict='UNKNOWN')))
        self.assertIsNone(et.result_failure_class(None, None))

    def test_selection_reads_results_and_provisional(self):
        with tempfile.TemporaryDirectory() as root:
            c = Path(root)
            for name in ('results', 'provisional'):
                (c / name).mkdir()
            rows = [dict(task=i, rank=10 + i, arm='tactix', tier='D', instance=f'i{i}',
                         expected='UNSAT') for i in range(4)]
            with (c / 'matrix.tsv').open('w', newline='') as f:
                w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter='\t')
                w.writeheader()
                w.writerows(rows)
            (c / 'results/000.json').write_text(json.dumps(unsat(stage('drat-trim', 137))))
            (c / 'results/001.json').write_text(json.dumps(unsat(stage('drat-trim', None, timed_out=True))))
            (c / 'provisional/002.json').write_text(json.dumps(dict(verdict='UNSAT')))
            (c / 'results/003.json').write_text(json.dumps(
                unsat(stage('cake_lpr', 0, result='verified'), valid=True)))
            chosen = et.recheck_selection(c, ('memout',))
            self.assertEqual([(int(r['task']), cls) for r, cls in chosen], [(0, 'memout')])
            chosen = et.recheck_selection(c, ('memout', 'timeout', 'unfinished'))
            self.assertEqual([cls for _, cls in chosen], ['memout', 'timeout', 'unfinished'])


class GovernorRegistration(unittest.TestCase):
    def test_registered_array_is_held_by_the_next_governor_tick(self):
        with tempfile.TemporaryDirectory() as root:
            d = Path(root)
            (d / 'jobs.json').write_text(json.dumps(
                {'jobs': [{'id': '100', 'label': 'baseline'}], 'expect_more': True,
                 'policy': 'kept'}))
            et.register_with_governor(d / 'jobs.json', '4242', 'tierd')
            et.register_with_governor(d / 'jobs.json', '4242', 'tierd')
            data = json.loads((d / 'jobs.json').read_text())
            self.assertEqual([j['id'] for j in data['jobs']], ['100', '4242'])
            self.assertEqual(data['policy'], 'kept')
            calls = []

            def run(cmd, **_kwargs):
                calls.append(cmd)
                out = ''
                if cmd[0] == 'squeue' and '-t' in cmd:
                    out = '4242_7|Priority\n4242_8|JobHeld\n'
                elif cmd[0] == 'squeue':
                    out = '4242_1\n'
                return subprocess.CompletedProcess(cmd, 0, out, '')
            args = mock.Mock(hold_above=750.0, release_below=700.0, user='u')
            qg.cycle(d, args, run=run, usage_reader=lambda: (800.0, 'test'))
            self.assertIn(['scontrol', 'hold', '4242_7'], calls)
            self.assertEqual(json.loads((d / 'state.json').read_text())['held'], ['4242_7'])
            record = json.loads((d / 'governor.log.jsonl').read_text().splitlines()[-1])
            self.assertIn('4242', record['jobs'])


class HomeScratchRuntime(unittest.TestCase):
    def campaign(self, root, solver_body):
        campaign = root / 'benchmarks/runs/home'
        cache = root / 'benchmarks/cache'
        for name in ('pins', 'claims', 'provisional', 'results', 'scratch'):
            (campaign / name).mkdir(parents=True)
        cache.mkdir(parents=True)
        instance = 'tiny.cnf.xz'
        (cache / instance).write_bytes(lzma.compress(b'p cnf 1 2\n1 0\n-1 0\n'))
        solver = campaign / 'pins/solver'
        solver.write_text('#!/usr/bin/env python3\nimport os,sys,time\n' + solver_body)
        solver.chmod(0o550)
        (campaign / 'pins/modelcheck.py').write_text('pin')
        for name, body in (('checkmodel', 'exit 0'),
                           ('drat-trim', 'echo "s VERIFIED"; exit 0')):
            (campaign / 'pins' / name).write_text('#!/bin/sh\n' + body + '\n')
            (campaign / 'pins' / name).chmod(0o550)
        (campaign / 'pins/directory_scratch.py').write_bytes(
            (HERE / 'directory_scratch.py').read_bytes())
        pins = {str(p.relative_to(campaign)): sha(p) for p in (campaign / 'pins').iterdir()}
        manifest = {
            'arms': {'tactix': {'path': 'pins/solver', 'sha256': sha(solver),
                                'arguments': [], 'certificate': 'drat'}},
            'input_sha256': {instance: sha(cache / instance)},
            'pin_sha256': pins,
            'scratch_policy': runtime_task.HOME_SCRATCH_POLICY,
            'scratch_directory': str(campaign / 'scratch'),
            'resource_contract': {'solve_timeout': 30, 'checker_timeout': 30,
                                  'cake_heap_mib': 57344},
        }
        (campaign / 'manifest.json').write_text(json.dumps(manifest))
        (campaign / 'matrix.tsv').write_text(
            'task\trank\tarm\ttier\tinstance\texpected\n'
            f'0\t1\ttactix\tD\t{instance}\tUNKNOWN\n')
        environment = dict(os.environ, SLURM_JOB_ID='unit', PYTHONDONTWRITEBYTECODE='1',
                           PYTHONPATH=str(HERE))
        return campaign, environment

    def test_certificate_lives_in_campaign_scratch_until_classified(self):
        for verdict in ('UNSAT', 'SAT'):
            with self.subTest(verdict=verdict), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                seen = root / 'seen.json'
                body = (f'json_path={str(seen)!r}\nimport json\n'
                        'open(sys.argv[-1],"wb").write(b"d 1 0\\n"*3)\n'
                        'json.dump(dict(proof=os.path.realpath(sys.argv[-1]),'
                        'cnf=os.path.realpath(sys.argv[-2]),tmp=os.environ["TMPDIR"]),'
                        'open(json_path,"w"))\n')
                body += ('print("s UNSATISFIABLE");sys.exit(20)\n' if verdict == 'UNSAT'
                         else 'print("s SATISFIABLE");print("v 1 0");sys.exit(10)\n')
                campaign, environment = self.campaign(root, body)
                completed = subprocess.run(
                    [sys.executable, HERE / 'runtime_task.py', campaign, '0'],
                    env=environment, capture_output=True, text=True, timeout=60)
                result = json.loads((campaign / 'results/000.json').read_text())
                self.assertEqual(completed.returncode, 0, (completed.stderr, result))
                where = json.loads(seen.read_text())
                task_dir = campaign / 'scratch/0000-unit'
                self.assertEqual(Path(where['proof']), task_dir / 'proof.drat')
                self.assertEqual(Path(where['cnf']), task_dir / 'input.cnf')
                self.assertEqual(Path(where['tmp']), task_dir / 'tmp')
                self.assertEqual(result['verdict'], verdict)
                self.assertTrue(result['valid'], result)
                self.assertEqual(result['certificate_bytes'], 18)
                self.assertTrue(result['artifact_deleted'])
                self.assertFalse(result['cleanup_pending'])
                status = result['scratch_resources']
                self.assertTrue(status['removed'])
                self.assertEqual(status['released_bytes']['proof.drat'], 18)
                self.assertEqual(list((campaign / 'scratch').iterdir()), [])
                if verdict == 'UNSAT':
                    self.assertEqual(result['proof_check']['result'], 'verified')
                    self.assertIn('checker.cnf', status['released_bytes'])

    def test_sigterm_kills_the_solver_and_removes_scratch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            started = root / 'started'
            body = (f'open(sys.argv[-1],"wb").write(b"x"*1000)\n'
                    f'open({str(started)!r},"w").write(str(os.getpid()))\n'
                    'time.sleep(60)\n')
            campaign, environment = self.campaign(root, body)
            process = subprocess.Popen(
                [sys.executable, HERE / 'runtime_task.py', campaign, '0'],
                env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            deadline = time.monotonic() + 30
            while not started.exists() and time.monotonic() < deadline:
                time.sleep(0.05)
            self.assertTrue(started.exists())
            solver_pid = int(started.read_text())
            process.send_signal(signal.SIGTERM)
            process.communicate(timeout=30)
            self.assertEqual(process.returncode, 1)
            result = json.loads((campaign / 'results/000.json').read_text())
            self.assertEqual(result['verdict'], 'INFRA_ERROR')
            self.assertIn('TaskTerminated', result['error'])
            self.assertTrue(result['scratch_resources']['removed'])
            self.assertTrue(result['artifact_deleted'])
            self.assertEqual(list((campaign / 'scratch').iterdir()), [])
            with self.assertRaises(ProcessLookupError):
                os.kill(solver_pid, 0)

    def test_cake_heap_follows_the_campaign_memory(self):
        seen = {}

        def fake_measured(command, stdout, stderr, metrics, timeout, environment=None):
            seen['heap'] = environment.get('CML_HEAP_SIZE')
            stdout.write_text('s VERIFIED UNSAT\n')
            return dict(exit_code=0, wall_seconds=0.0, peak_rss_kb=1, timed_out=False)
        with tempfile.TemporaryDirectory() as directory:
            scratch = DirectoryScratch(directory, 'task')
            self.addCleanup(scratch.close)
            for heap, expected in ((57344, '57344'), (None, '24576')):
                with mock.patch.object(runtime_task, 'CAKE_HEAP_MIB', heap), \
                        mock.patch.object(runtime_task, 'measured', fake_measured):
                    result = runtime_task.proof_stage(scratch, 'cake_lpr', ['x'], 10)
                self.assertEqual(result['result'], 'verified')
                self.assertEqual(seen['heap'], expected)
        self.assertEqual(runtime_task.cake_heap_mib({'resource_contract': {}}), None)
        with self.assertRaises(RuntimeError):
            runtime_task.cake_heap_mib({'resource_contract': {'cake_heap_mib': 100}})


class VerifiedCertificateIdentity(unittest.TestCase):
    """A certificate byte-identical to a verified reference skips its checker."""

    def run_with_reference(self, reference_digest):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            body = ('open(sys.argv[-1],"wb").write(b"d 1 0\\n"*3)\n'
                    'print("s UNSATISFIABLE");sys.exit(20)\n')
            campaign, environment = HomeScratchRuntime.campaign(None, root, body)
            # The checker rejects everything, so only identity can verify.
            (campaign / 'pins/drat-trim').chmod(0o750)
            (campaign / 'pins/drat-trim').write_text('#!/bin/sh\necho "s NOT VERIFIED"; exit 1\n')
            table = campaign / 'pins/verified-certificates.json'
            table.write_text(json.dumps({'tiny.cnf.xz': [dict(
                certificate_sha256=reference_digest, campaign='ref', task=0,
                binary_sha256='b', pipeline='dsr-as-drat-drat-trim-cake_lpr')]}))
            manifest = json.loads((campaign / 'manifest.json').read_text())
            manifest['pin_sha256'] = {str(p.relative_to(campaign)): sha(p)
                                      for p in (campaign / 'pins').iterdir()}
            manifest['verified_certificate_reference'] = dict(
                path='pins/verified-certificates.json', sha256=sha(table))
            (campaign / 'manifest.json').write_text(json.dumps(manifest))
            completed = subprocess.run(
                [sys.executable, HERE / 'runtime_task.py', campaign, '0'],
                env=environment, capture_output=True, text=True, timeout=60)
            result = json.loads((campaign / 'results/000.json').read_text())
            self.assertEqual(list((campaign / 'scratch').iterdir()), [])
            return completed.returncode, result

    def test_identical_certificate_is_verified_without_checking(self):
        digest = hashlib.sha256(b'd 1 0\n' * 3).hexdigest()
        returncode, result = self.run_with_reference(digest)
        self.assertEqual(returncode, 0, result)
        self.assertTrue(result['valid'], result)
        self.assertEqual(result['proof_check']['pipeline'], 'reference-certificate-identity')
        self.assertEqual(result['proof_check']['reference']['certificate_sha256'], digest)

    def test_different_certificate_is_checked(self):
        returncode, result = self.run_with_reference('0' * 64)
        self.assertNotEqual(returncode, 0, result)
        self.assertFalse(result['valid'], result)
        self.assertNotEqual(result['proof_check'].get('pipeline'),
                            'reference-certificate-identity')


class RecheckPreparation(unittest.TestCase):
    def test_memout_rows_become_a_64g_home_recheck(self):
        frozen = REPO / 'benchmarks/baselines/sc24-26-32g-5000s/manifest.json'
        if not frozen.exists():
            self.skipTest('frozen Tier D baseline is not available')
        instances = json.loads(frozen.read_text())['instances'][:3]
        commit = subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', 'HEAD'],
                                         text=True).strip()
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'
            for name in ('pins', 'results', 'provisional'):
                (source / name).mkdir(parents=True)
            binary = source / 'pins/tactix'
            binary.write_text('#!/bin/sh\nexit 0\n')
            rows = [dict(task=i, rank=i + 1, arm='tactix', tier='D', instance=r['rel'],
                         expected=r['expected']) for i, r in enumerate(instances)]
            with (source / 'matrix.tsv').open('w', newline='') as f:
                w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter='\t')
                w.writeheader()
                w.writerows(rows)
            (source / 'results/000.json').write_text(json.dumps(
                unsat(stage('dsr-trim', 255, 'Ran out of memory on xrealloc()'))))
            (source / 'results/001.json').write_text(json.dumps(
                unsat(stage('cake_lpr', 0, result='verified'), valid=True)))
            (source / 'results/002.json').write_text(json.dumps(
                unsat(stage('drat-trim', None, timed_out=True))))
            (source / 'manifest.json').write_text(json.dumps(dict(
                arms={'tactix': dict(path='pins/tactix', sha256=sha(binary),
                                     arguments=['--auto-proof', '--default-graph-labeling=on'],
                                     certificate='auto')},
                source_commit=commit, source_tree=str(REPO / 'missing-source-tree'),
                source_files={'src/x.cpp': '0' * 64}, dsr_checker_profile='long64-fixed',
                pin_sha256={'pins/tactix': sha(binary)})))
            campaign = Path(directory) / 'recheck'
            completed = subprocess.run(
                [sys.executable, HERE / 'experimental_tierd.py', '--prepare',
                 '--campaign', campaign, '--recheck-from', source,
                 '--recheck-classes', 'memout', '--home-scratch', '--memory-mib', '65536',
                 '--partition', 'cpu-q', '--cpus-per-task', '4', '--solve-timeout', '5000',
                 '--checker-timeout', '28800'],
                capture_output=True, text=True, timeout=600,
                env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))
            self.assertEqual(completed.returncode, 0, completed.stderr)
            m = json.loads((campaign / 'manifest.json').read_text())
            self.assertEqual(m['recheck_source']['counts'], {'memout': 1})
            self.assertEqual(m['resource_contract']['memory_mib'], 65536)
            self.assertEqual(m['resource_contract']['cake_heap_mib'], 57344)
            self.assertEqual(m['resource_contract']['checker_timeout'], 28800)
            self.assertEqual(m['scratch_policy'], 'home-directory-files-v1')
            self.assertEqual(m['scratch_directory'], str(campaign / 'scratch'))
            self.assertEqual(m['arms']['tactix']['arguments'],
                             ['--auto-proof', '--default-graph-labeling=on'])
            self.assertEqual(m['arms']['tactix']['sha256'], sha(binary))
            self.assertEqual(m['source_commit'], commit)
            self.assertEqual(m['dsr_checker_profile'], 'long64-fixed')
            with (campaign / 'matrix.tsv').open(newline='') as f:
                matrix = list(csv.DictReader(f, delimiter='\t'))
            self.assertEqual([(r['task'], r['rank'], r['source_task'], r['source_class'])
                              for r in matrix], [('0', '1', '0', 'memout')])
            array = (campaign / 'array.sbatch').read_text()
            self.assertIn('#SBATCH --mem=65536M', array)
            self.assertIn('#SBATCH --array=0-0\n', array)
            self.assertIn('#SBATCH --time=09:45:00', array)
            self.assertIn('pins/test_directory_scratch.py',
                          (campaign / 'validate.sbatch').read_text())
            for rel in ('pins/directory_scratch.py', 'pins/srcheck', 'pins/veripb'):
                self.assertIn(rel, m['pin_sha256'])
            # Nothing selectable is refused rather than prepared empty.
            again = subprocess.run(
                [sys.executable, HERE / 'experimental_tierd.py', '--prepare',
                 '--campaign', Path(directory) / 'none', '--recheck-from', source,
                 '--recheck-classes', 'crash', '--home-scratch'],
                capture_output=True, text=True, timeout=120)
            self.assertNotEqual(again.returncode, 0)
            self.assertIn('recheck selection is empty', again.stderr)


class CertificateNone(unittest.TestCase):
    """certificate none: proof-free solving, models checked, no proof pins."""

    def test_solver_gets_no_proof_path_or_flag(self):
        self.assertEqual(et.solver_arguments('none', []), [])
        self.assertEqual(et.solver_arguments('dsr', []), ['--dsr'])
        self.assertEqual(et.solver_arguments('auto', ['--x']), ['--auto-proof', '--x'])
        arm = dict(arguments=[], certificate='none')
        self.assertEqual(runtime_task.solver_command('b', arm, 'f.cnf', 'p'), ['b', 'f.cnf'])
        arm = dict(arguments=['--dsr'], certificate='dsr')
        self.assertEqual(runtime_task.solver_command('b', arm, 'f.cnf', 'p'),
                         ['b', '--dsr', 'f.cnf', 'p'])

    def test_only_model_checker_pins_are_required(self):
        ref = REPO / 'benchmarks/baselines/sc24-26-32g-5000s'
        manifest = json.loads((ref / 'manifest.json').read_text())
        self.assertEqual(set(et.checker_sources(ref, manifest, 'none')),
                         {'checkmodel', 'modelcheck.py'})
        with tempfile.TemporaryDirectory() as d:
            c = Path(d)
            (c / 'pins').mkdir()
            for name in ('tactix', 'checkmodel', 'modelcheck.py'):
                (c / 'pins' / name).write_text('')
            pins = {f'pins/{n}': sha(c / 'pins' / n)
                    for n in ('tactix', 'checkmodel', 'modelcheck.py')}
            runtime_task.validate_runtime_pin_contract(
                c, dict(arms={'tactix': dict(path='pins/tactix', certificate='none')},
                        pin_sha256=pins))


if __name__ == '__main__':
    unittest.main()
