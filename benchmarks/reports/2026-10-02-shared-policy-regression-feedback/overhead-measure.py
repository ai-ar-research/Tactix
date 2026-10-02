"""Paired frontend-feedback overhead on declared fixed synthetic cases."""
import hashlib
import json
import os
from pathlib import Path
import random
import re
import shutil
import signal
import statistics
import subprocess
import sys
import time

RUN = Path(__file__).resolve().parent
sys.path.insert(0, str(RUN / 'pins'))
from anonymous_scratch import AnonymousScratch
from inspect_policy_feedback import inspect
from modelcheck import check_files
from runtime_task import check_unsat_proof, proof_pipeline_deadline
from proof_oracle import solver_reported_route_text


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixtures():
    result = []
    def add(name, variables, clauses, verdict):
        cnf = 'p cnf %d %d\n' % (variables, len(clauses))
        cnf += ''.join(' '.join(map(str, clause)) + ' 0\n' for clause in clauses)
        result.append({'name': name, 'cnf': cnf, 'expected_exit': verdict,
                       'input_sha256': hashlib.sha256(cnf.encode()).hexdigest()})
    add('binary-chain-sat', 128, [[-v, v + 1] for v in range(1, 128)], 10)
    rng = random.Random(913)
    clauses = []
    for _ in range(130):
        clause = [v if rng.randrange(2) else -v for v in rng.sample(range(1, 33), 3)]
        if all(lit < 0 for lit in clause): clause[0] *= -1
        clauses.append(clause)
    add('planted-3sat', 32, clauses, 10)
    for pigeons in (4, 6):
        holes = 5
        cell = lambda p, h: p * holes + h + 1
        clauses = [[cell(p, h) for h in range(holes)] for p in range(pigeons)]
        clauses += [[-cell(p, h), -cell(q, h)] for p in range(pigeons)
                    for q in range(p + 1, pigeons) for h in range(holes)]
        add(f'php-{pigeons}-5', pigeons * holes, clauses, 10 if pigeons == 4 else 20)
    return result


def verify(manifest):
    for name, expected in manifest['sha256'].items():
        assert digest(Path(name)) == expected, name


def stable_output(stdout):
    # Native wall measurements and the frontend's elapsed line are evidence,
    # not state equality. All other emitted statistics and model tokens match.
    stdout = re.sub(r'(c handoff[^\n]*, )[0-9.]+ seconds', r'\1<elapsed> seconds', stdout)
    return '\n'.join(line for line in stdout.splitlines()
                     if not line.startswith(('c timing ', 'c phasecost ', 'c time ')))


def measure_solver(command, stdout, stderr, metrics, timeout, environment):
    # Use blocking wait plus a wall alarm. Timeout polling can quantize short
    # runs by exponential sleep intervals and obscure the cost being measured.
    started = time.perf_counter_ns()
    with stdout.open('wb') as out, stderr.open('wb') as err:
        process = subprocess.Popen(['/usr/bin/time', '-f', 'max_rss_kb=%M', '-o', str(metrics), *command],
                                   stdout=out, stderr=err, env=environment, start_new_session=True)
        try:
            with proof_pipeline_deadline(timeout):
                status = process.wait()
            timed_out = False
        except BaseException:
            try: os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError: pass
            process.wait()
            raise
    elapsed = (time.perf_counter_ns() - started) / 1e9
    match = re.search(r'^max_rss_kb=(\d+)$', metrics.read_text(), re.M)
    return {'exit_code': status, 'timed_out': timed_out, 'wall_seconds': round(elapsed, 6),
            'peak_rss_kb': int(match[1]) if match else None}


def main():
    manifest = json.loads((RUN / 'manifest.json').read_text())
    verify(manifest)
    inputs = fixtures()
    assert [{k: v for k, v in case.items() if k != 'cnf'} for case in inputs] == manifest['fixtures']
    env = {k: v for k, v in os.environ.items() if not k.startswith('TACTIX_')}
    env['TACTIX_STATS_TIMING'] = '1'
    expected = {}
    rows = []
    certificates = {}
    try:
        for case in inputs:
            for policy, arguments in manifest['policies'].items():
                for repeat in range(-1, manifest['pairs_per_case']):
                    arms = ('off', 'on') if repeat % 2 == 0 else ('on', 'off')
                    for arm in arms:
                        scratch = AnonymousScratch()
                        try:
                            cnf, proof = scratch / 'input.cnf', scratch / 'proof'
                            stdout, stderr, metrics = (scratch / name for name in ('stdout', 'stderr', 'metrics'))
                            cnf.write_text(case['cnf'])
                            command = [manifest['solver'], '--auto-proof', *arguments]
                            feedback = scratch / 'feedback'
                            if arm == 'on': command.append('--feedback=' + str(feedback))
                            command.extend([str(cnf), str(proof)])
                            measurement = measure_solver(command, stdout, stderr, metrics,
                                                   manifest['solver_timeout_seconds'], environment=env)
                            assert not measurement['timed_out'], (case['name'], policy, arm, 'timeout')
                            assert measurement['exit_code'] == case['expected_exit'], (case['name'], policy, arm, measurement, stderr.read_text())
                            output = stdout.read_text()
                            signature = {'stdout_sha256': hashlib.sha256(stable_output(output).encode()).hexdigest(),
                                         'proof_sha256': digest(proof)}
                            key = (case['name'], policy)
                            if key not in expected: expected[key] = signature
                            assert signature == expected[key], (key, repeat, arm, 'deterministic effects differ')
                            certificate = (case['input_sha256'], signature['proof_sha256'], case['expected_exit'])
                            if certificate not in certificates:
                                if case['expected_exit'] == 10:
                                    ok, message = check_files(str(cnf), str(stdout))
                                    check = {'kind': 'SAT', 'verified': ok, 'message': message}
                                else:
                                    check = check_unsat_proof(Path(manifest['checker_campaign']), 'auto', cnf, proof,
                                                             scratch, manifest['checker_timeout_seconds'],
                                                             reported_route=solver_reported_route_text(output))
                                    check['kind'] = 'UNSAT'
                                    ok = check['result'] == 'verified' and not check['timed_out']
                                assert ok, (key, check)
                                certificates[certificate] = check
                            # SAT assignments are checked each time, since a
                            # proof hash alone cannot identify the printed model.
                            if case['expected_exit'] == 10:
                                ok, message = check_files(str(cnf), str(stdout))
                                assert ok, (key, repeat, arm, message)
                            feedback_summary = None
                            if arm == 'on':
                                checked = inspect(feedback)
                                feedback_summary = {'bytes': feedback.stat().st_size,
                                    'actions': sum(value['records'] for value in checked['actions'].values()),
                                    'intervals': sum(value if isinstance(value, int) else value['records']
                                                     for value in checked['intervals'].values())}
                            rows.append({'case': case['name'], 'policy': policy, 'repeat': repeat,
                                         'warmup': repeat < 0, 'arm': arm, 'measurement': measurement,
                                         'signature': signature, 'feedback': feedback_summary})
                            (RUN / 'partial.json').write_text(json.dumps(rows, indent=2) + '\n')
                        except BaseException:
                            bundle = RUN / f"failed-{case['name']}-{policy}-{repeat}-{arm}"
                            bundle.mkdir()
                            for name in ('input.cnf', 'proof', 'stdout', 'stderr', 'feedback', 'metrics'):
                                if name in scratch.handles:
                                    shutil.copyfile(Path(f'/proc/{os.getpid()}/fd/{scratch.handles[name]}'), bundle / name)
                            raise
                        finally:
                            scratch.close()
        verify(manifest)
        summaries = []
        for case in inputs:
            for policy in manifest['policies']:
                selected = [row for row in rows if row['case'] == case['name'] and row['policy'] == policy and not row['warmup']]
                off = [row['measurement']['wall_seconds'] for row in selected if row['arm'] == 'off']
                on = [row['measurement']['wall_seconds'] for row in selected if row['arm'] == 'on']
                assert len(off) == len(on) == manifest['pairs_per_case']
                summaries.append({'case': case['name'], 'policy': policy, 'pairs': len(off),
                    'off_median_seconds': statistics.median(off), 'on_median_seconds': statistics.median(on),
                    'additional_median_seconds': statistics.median(on) - statistics.median(off),
                    'median_ratio': statistics.median(on) / statistics.median(off),
                    'off_range_seconds': [min(off), max(off)], 'on_range_seconds': [min(on), max(on)]})
        result = {'schema': 'tactix-feedback-frontend-overhead-v1', 'accepted_measurement': True,
                  'scope': manifest['scope'], 'node': os.environ.get('SLURMD_NODENAME'),
                  'affinity': sorted(os.sched_getaffinity(0)), 'rows': rows, 'summaries': summaries,
                  'independent_certificate_checks': [{'key': list(key), 'check': check} for key, check in certificates.items()],
                  'deterministic_signatures_equal': True, 'pins_verified_before_after': True}
        (RUN / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps({'completed_runs': len(rows), 'paired_cases': len(summaries), 'summaries': summaries}), flush=True)
    except BaseException:
        (RUN / 'failure-marker.txt').write_text('Measurement incomplete; inspect exception and partial rows.\n')
        raise


if __name__ == '__main__': main()
