#!/usr/bin/env python3
"""Full Tier D winner task using the established anonymous solver adapters."""

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import subprocess
import sys

from anonymous_scratch import AnonymousScratch
import portfolio_rerun as prior


prior.CPUS = 4


def run_case(campaign, manifest, arm, compressed=None, formula=None,
             solve_timeout=None, checker_timeout=None):
    scratch = AnonymousScratch('/tmp')
    try:
        cnf = scratch / 'input.cnf'
        if compressed is not None:
            with open(cnf, 'wb') as target:
                subprocess.run(['xz', '-dc', compressed], stdout=target, check=True,
                               env=dict(manifest['solver_environment']))
        else:
            cnf.write_text(formula)
        result = prior.execute_case(
            campaign, manifest, arm, 'UNKNOWN', cnf, scratch,
            solve_timeout=solve_timeout or manifest['resource_contract']['solve_timeout'],
            check_timeout=checker_timeout or manifest['resource_contract']['checker_timeout'])
        return result
    finally:
        scratch.close()
        if scratch.exists():
            raise RuntimeError('anonymous scratch handles survived cleanup')


def validate(campaign):
    manifest = json.loads((campaign / 'manifest.json').read_text())
    cases = [('SAT', 'p cnf 2 2\n1 2 0\n-1 2 0\n'),
             ('UNSAT', 'p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n')]
    checks = []
    for arm in manifest['arms']:
        for expected, formula in cases:
            try:
                result = run_case(campaign, manifest, arm, formula=formula,
                                  solve_timeout=120, checker_timeout=120)
                check = {'arm': arm, 'expected': expected,
                         'verdict': result['verdict'],
                         'solve': result['solve'],
                         'solver_stderr_tail': result['solver_stderr_tail'],
                         'proof_check': result['proof_check'],
                         'model_check': result['model_check'],
                         'passed': result['valid'] and result['verdict'] == expected}
            except Exception as error:
                check = {'arm': arm, 'expected': expected,
                         'passed': False, 'error': repr(error)}
            checks.append(check)
    for code in ('import os; os._exit(7)', 'import time; time.sleep(60)'):
        scratch = AnonymousScratch('/tmp')
        try:
            abnormal = prior.measured(
                [sys.executable, '-c', code], scratch / 'out', scratch / 'err',
                scratch / 'time', 0.1, dict(manifest['solver_environment']))
        finally:
            scratch.close()
        checks.append({'abnormal': abnormal, 'cleanup': not scratch.exists(),
                       'passed': (abnormal['exit_code'] == 7 or abnormal['timed_out'])
                       and not scratch.exists()})
    passed = all(check['passed'] for check in checks)
    prior.atomic_json(campaign / 'validation.json', {
        'passed': passed, 'job_id': os.environ['SLURM_JOB_ID'],
        'node': os.environ.get('SLURMD_NODENAME', os.uname().nodename),
        'manifest_sha256': prior.sha256(campaign / 'manifest.json'),
        'checks': checks}, exclusive=True)
    print('PASS' if passed else 'FAIL', 'four-winner anonymous validation')
    return 0 if passed else 1


def run_task(campaign, task):
    manifest = json.loads((campaign / 'manifest.json').read_text())
    row = manifest['matrix'][task]
    claim = campaign / 'claims' / f'{task:05d}.json'
    prior.atomic_json(claim, {
        'task': task, 'job_id': os.environ['SLURM_JOB_ID'],
        'array_job_id': os.environ['SLURM_ARRAY_JOB_ID'],
        'node': os.environ.get('SLURMD_NODENAME', os.uname().nodename),
        'started_utc': dt.datetime.now(dt.timezone.utc).isoformat()}, exclusive=True)
    if prior.sha256(campaign / 'pins/experimental_winner_task.py') != manifest['runner_sha256']:
        raise RuntimeError('winner runner hash mismatch')
    compressed = Path(manifest['cache_root']) / Path(row['instance']).name
    if prior.sha256(compressed) != manifest['input_sha256'][row['instance']]:
        raise RuntimeError('compressed input hash mismatch')
    try:
        result = run_case(campaign, manifest, row['arm'], compressed=compressed)
    except Exception as error:
        result = {'verdict': 'INFRA_ERROR', 'valid': False, 'error': repr(error),
                  'solve': None, 'proof_check': None, 'model_check': None}
    result.update({'schema': 'experimental-four-winner-tierd-v1',
                   'task': task, 'rank': row['rank'], 'arm': row['arm'],
                   'instance': row['instance'], 'expected': row['expected'],
                   'node': os.environ.get('SLURMD_NODENAME', os.uname().nodename),
                   'array_job_id': os.environ['SLURM_ARRAY_JOB_ID'],
                   'artifact_deleted': True, 'derived_artifacts_deleted': True,
                   'cleanup_pending': False,
                   'finished_utc': dt.datetime.now(dt.timezone.utc).isoformat()})
    result['expected_match'] = prior.expected_matches(row['expected'], result['verdict'])
    result['valid'] = result['valid'] and result['expected_match']
    prior.atomic_json(campaign / 'results' / f'{task:05d}.json', result, exclusive=True)
    return 0 if result['valid'] or result['verdict'] == 'UNKNOWN' else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('campaign', type=Path)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--validate', action='store_true')
    group.add_argument('--task', type=int)
    args = parser.parse_args()
    campaign = args.campaign.resolve()
    return validate(campaign) if args.validate else run_task(campaign, args.task)


if __name__ == '__main__':
    sys.exit(main())
