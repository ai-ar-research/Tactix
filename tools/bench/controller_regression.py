#!/usr/bin/env python3
"""Prepare and score the paper-target bounded controller regression gates.

The preserved paper release is the per-instance authority and aggregate solver
time may be at most ten percent slower.  Censored paper outcomes remain explicit
exceptions rather than becoming independent verification.
"""
import argparse
from collections import Counter
import csv
import datetime as dt
import json
import math
from pathlib import Path
from proof_oracle import pin_dsr_transport
import shlex
import shutil
import subprocess

from fixed_runtime import CANONICAL_CHECKERS, sha, require

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / 'benchmarks/baselines/controller-paper-v1/results.json'
BASELINE_FREEZER = ROOT / 'tools/bench/freeze_paper_controller_baseline.py'
CHECK_TIMEOUT = 1800


def within_runtime_target(total, ceiling):
    """The paper target permits equality and no more than ten percent excess."""
    return ceiling is None or total <= ceiling


def score_target_path(path):
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError:
        return str(path.resolve())


def write_score(path, result, replace=False):
    if path.exists():
        require(replace, 'score output already exists')
        prior = json.loads(path.read_text())
        require(prior.get('schema') == result['schema'] and
                prior.get('manifest_sha256') == result['manifest_sha256'] and
                prior.get('target_baseline_sha256') == result['target_baseline_sha256'],
                'refusing to replace score for a different campaign or target')
    temporary = path.with_name('.' + path.name + '.tmp')
    require(not temporary.exists(), 'temporary score output already exists')
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(path)


def proof_cost_comparison(candidate, baseline):
    """Describe the measured comparison without inventing censored completion."""
    elapsed = (candidate or {}).get('wall_seconds')
    original = (baseline or {}).get('wall_seconds')
    comparable = bool(baseline and baseline.get('result') == 'verified' and
                      not baseline.get('timed_out'))
    limit = original + max(0.30 * original, 20) if comparable else None
    if not candidate:
        status = 'missing'
    elif not isinstance(elapsed, (int, float)) or not math.isfinite(elapsed):
        status = 'invalid-measurement'
    elif proof_disposition(candidate, None)[0] == 'failed':
        status = 'checker-failure'
    elif not comparable:
        status = 'inconclusive'
    elif elapsed > limit:
        status = 'significant-slowdown'
    elif candidate.get('timed_out'):
        status = 'censored-within-bound'
    else:
        status = 'completed-within-bound'
    return dict(status=status, baseline_wall_seconds=original,
                candidate_wall_seconds=elapsed, allowed_completion_seconds=limit,
                baseline_censored=bool(baseline and baseline.get('timed_out')),
                candidate_censored=bool(candidate and candidate.get('timed_out')))


def candidate_arguments(arguments=()):
    """Keep this Default-policy gate on its original certificate contract.

    Extensions are deliberately enumerated: arbitrary CLI policy/proof flags
    would silently change what the historical comparisons qualify.
    """
    values = list(arguments)
    allowed = {
        '--default-affine-search=on', '--default-affine-search=off',
        '--default-graph-labeling=on', '--default-graph-labeling=off',
    }
    require(all(isinstance(value, str) and value in allowed for value in values),
            'unsupported qualified Default solver argument')
    families = [value.split('=', 1)[0] for value in values]
    require(len(families) == len(set(families)),
            'duplicate or conflicting qualified Default argument')
    return ['--auto-proof', *values]


def prepare(prefix, release, solver_arguments=(), dsr_transport=None):
    arguments = candidate_arguments(solver_arguments)
    require(release.is_relative_to(ROOT), 'candidate must be in repository')
    pin = json.loads((release.parent / 'PIN.json').read_text())
    require(pin['build_type'] == 'Release', 'production Release required')
    require(sha(release) == pin['binaries']['tactix'], 'candidate pin changed')
    subprocess.run(['python3', str(BASELINE_FREEZER),
                    '--verify'], check=True)
    baseline = json.loads(BASELINE.read_text())
    campaigns = []
    for name, accepted in baseline['sets'].items():
        campaign = ROOT / 'benchmarks/runs' / (prefix + '-' + name)
        require(not campaign.exists(), 'campaign exists: ' + str(campaign))
        source = ROOT / accepted['source_manifest']
        require(sha(source) == accepted['source_manifest_sha256'], 'baseline changed')
        old = json.loads(source.read_text())
        require(len(old['arms']) == 1, 'baseline arm ambiguity')
        arm_name, old_arm = next(iter(old['arms'].items()))
        require(old_arm['arguments'] == ['--auto-proof'], 'unexpected baseline arguments')
        for instance, digest in accepted['input_sha256'].items():
            require(sha(ROOT / 'benchmarks/cache' / instance) == digest, 'input changed')
        campaign.mkdir()
        for directory in ('pins', 'logs', 'results', 'provisional', 'claims'):
            (campaign / directory).mkdir()
        for filename in ('runtime_task.py', 'proof_oracle.py', 'anonymous_scratch.py'):
            shutil.copy2(ROOT / 'tools/bench' / filename, campaign / 'pins' / filename)
        pin_dsr_transport(dsr_transport, campaign / 'pins')
        for filename in ('checkmodel', 'modelcheck.py', *CANONICAL_CHECKERS):
            original = source.parent / 'pins' / filename
            expected = accepted['pin_sha256']['pins/' + filename]
            require(sha(original) == expected, 'baseline checker changed: ' + filename)
            if filename in CANONICAL_CHECKERS:
                require(expected == CANONICAL_CHECKERS[filename], 'noncanonical checker')
            shutil.copy2(original, campaign / 'pins' / filename)
        shutil.copy2(release, campaign / 'pins/tactix')
        shutil.copy2(release.parent / 'PIN.json', campaign / 'pins/candidate-pin.json')
        shutil.copy2(BASELINE, campaign / 'pins/baseline.json')
        shutil.copy2(source.parent / 'matrix.tsv', campaign / 'matrix.tsv')
        require(sha(campaign / 'matrix.tsv') == accepted['matrix_sha256'], 'matrix changed')
        contract = dict(accepted['resource_contract'])
        contract.update(checker_timeout=CHECK_TIMEOUT, checker_timeout_overrides={},
                        array=f"0-{accepted['expected_rows'] - 1}", throttle=None,
                        node_constraint=None)
        batch = f'''#!/bin/bash
#SBATCH --job-name=controller-{name}
#SBATCH --partition={contract['partition']}
#SBATCH --ntasks=1
#SBATCH --cpus-per-task={contract['cpus_per_task']}
#SBATCH --mem={contract['memory_mib']}M
#SBATCH --time=01:10:00
#SBATCH --array={contract['array']}
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
exec python3 {shlex.quote(str(campaign / 'pins/runtime_task.py'))} {shlex.quote(str(campaign))} "$SLURM_ARRAY_TASK_ID"
'''
        (campaign / 'array.sbatch').write_text(batch)
        manifest = dict(schema='tactix-controller-regression-v1', set=name,
            created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
            baseline=dict(path=str(BASELINE.relative_to(ROOT)), sha256=sha(BASELINE)),
            baseline_manifest=dict(path=str(source.relative_to(ROOT)), sha256=sha(source)),
            arms={arm_name: dict(path='pins/tactix', sha256=sha(release),
                source_commit=pin['source_commit'], source_build=str(release.parent.relative_to(ROOT)),
                arguments=arguments, certificate='auto')},
            resource_contract=contract, input_sha256=accepted['input_sha256'],
            collect_runtime_context=old.get('collect_runtime_context', False),
            scratch_policy='anonymous-local-files-v1',
            regression_contract='paper release plus ten percent aggregate solver time',
            limitation='Paper-target comparison; clean censored proofs are not independently verified.',
            acceptance_runtime_seconds=accepted['acceptance_runtime_seconds'])
        manifest['pin_sha256'] = {str(p.relative_to(campaign)): sha(p)
            for p in [*(campaign / 'pins').iterdir(), campaign / 'matrix.tsv', campaign / 'array.sbatch']}
        (campaign / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        campaigns.append(str(campaign))
    return campaigns


def proof_disposition(candidate, baseline):
    """Failures dominate clean timeout; completed baseline supplies a cost bound."""
    if not candidate:
        return 'missing', ['missing proof-check outcome']
    stages = candidate.get('stages', [])
    if not stages or any(s.get('result') != 'verified' and not s.get('timed_out')
                         for s in stages):
        return 'failed', ['checker rejection/error or missing stage evidence']
    elapsed = candidate.get('wall_seconds')
    if not isinstance(elapsed, (int, float)) or not math.isfinite(elapsed) or elapsed < 0:
        return 'failed', ['invalid checker elapsed time']
    if candidate.get('timed_out'):
        if candidate.get('result') == 'verified' or not any(s.get('timed_out') for s in stages):
            return 'failed', ['contradictory checker timeout evidence']
        disposition = 'censored'
    elif candidate.get('result') == 'verified' and all(s['result'] == 'verified' for s in stages):
        disposition = 'verified'
        endings = {'dsr-as-drat-drat-trim-cake_lpr': 'cake_lpr',
                   'dsr-trim-dsr-compress-trestle': 'srcheck',
                   'veripb-cake-pb': 'cake-pb-cnf'}
        if stages[-1]['name'] != endings.get(candidate.get('pipeline')):
            return 'failed', ['missing final independent verifier']
    else:
        return 'failed', ['checker did not verify or cleanly time out']
    errors = []
    if elapsed > CHECK_TIMEOUT + 1:
        errors.append('proof pipeline exceeded shared deadline')
    if baseline and baseline.get('result') == 'verified' and not baseline.get('timed_out'):
        original = baseline['wall_seconds']
        if elapsed > original + max(0.30 * original, 20):
            errors.append('significant proof-check cost regression')
    return disposition, errors


def score_row(row, accepted, contract):
    errors = []
    if row.get('error') or row.get('cleanup_pending') or not row.get('artifact_deleted') or not row.get('derived_artifacts_deleted'):
        errors.append('infrastructure error or incomplete scratch cleanup')
    solve = row.get('solve') or {}
    verdict = row.get('verdict')
    if solve.get('timed_out'):
        elapsed = solve.get('wall_seconds')
        if (verdict != 'UNKNOWN' or solve.get('exit_code') is not None or
                not isinstance(elapsed, (int, float)) or
                not math.isfinite(elapsed) or elapsed < 0 or
                elapsed > contract['solve_timeout'] + 1):
            errors.append('invalid solver-timeout evidence')
        if accepted['require_solver_retention']:
            errors.append('lost retained solve')
        # A saved solver-timeout exception may remain a clean timeout.
        if row.get('proof_check'):
            _, proof_errors = proof_disposition(row['proof_check'], accepted.get('proof_check'))
            errors.extend(proof_errors)
        return 'solver-timeout', errors
    if verdict not in ('SAT', 'UNSAT') or solve.get('exit_code') != {'SAT': 10, 'UNSAT': 20}.get(verdict):
        errors.append('solver failure or inconsistent exit/verdict')
    if verdict != accepted['expected'] or not row.get('expected_match'):
        errors.append('incorrect verdict')
    elapsed, rss = solve.get('wall_seconds'), solve.get('peak_rss_kb')
    if not isinstance(elapsed, (int, float)) or not math.isfinite(elapsed) or elapsed < 0 or elapsed > contract['solve_timeout']:
        errors.append('solve-time contract violated')
    if not isinstance(rss, (int, float)) or not math.isfinite(rss) or rss <= 0 or rss > contract['memory_mib'] * 1024:
        errors.append('memory contract violated or measurement missing')
    if verdict == 'SAT':
        model = row.get('model_check') or {}
        if model.get('result') != 'verified' or model.get('exit_code') != 0 or model.get('timed_out'):
            errors.append('model not independently verified')
        return 'model-verified', errors
    disposition, proof_errors = proof_disposition(row.get('proof_check'), accepted.get('proof_check'))
    return disposition, errors + proof_errors


def score(campaign, target_baseline=None):
    manifest = json.loads((campaign / 'manifest.json').read_text())
    require(manifest['schema'] == 'tactix-controller-regression-v1', 'wrong manifest')
    require(len(manifest['arms']) == 1, 'candidate arm ambiguity')
    arm = next(iter(manifest['arms'].values()))
    wire = arm.get('arguments')
    require(isinstance(wire, list) and wire and wire[0] == '--auto-proof' and
            wire == candidate_arguments(wire[1:]) and arm.get('certificate') == 'auto',
            'candidate certificate/argument contract changed')
    for relative, digest in manifest['pin_sha256'].items():
        require(sha(campaign / relative) == digest, 'campaign pin changed: ' + relative)
    if target_baseline is None:
        baseline_path = campaign / 'pins/baseline.json'
        require(sha(baseline_path) == manifest['baseline']['sha256'], 'campaign baseline changed')
        baseline = json.loads(baseline_path.read_text())
        target_kind = 'campaign-pinned'
    else:
        baseline_path = target_baseline
        baseline = json.loads(baseline_path.read_text())
        target_kind = 'paper-release-plus-10-percent'
    require(manifest['set'] in baseline['sets'], 'set absent from target baseline')
    accepted = baseline['sets'][manifest['set']]
    contract = manifest['resource_contract']
    require(contract['checker_timeout'] == CHECK_TIMEOUT and
            not contract.get('checker_timeout_overrides') and
            contract['throttle'] is None and contract['node_constraint'] is None,
            'bounded/full-width regression contract changed')
    for key in ('partition', 'cpus_per_task', 'memory_mib', 'solve_timeout', 'one_solver_per_task'):
        require(contract[key] == accepted['resource_contract'][key], 'resource mismatch: ' + key)
    with (campaign / 'matrix.tsv').open() as stream:
        matrix = list(csv.DictReader(stream, delimiter='\t'))
    require(len(matrix) == accepted['expected_rows'], 'matrix row count')
    require(sha(campaign / 'matrix.tsv') == accepted['matrix_sha256'], 'target matrix mismatch')
    require(manifest['input_sha256'] == accepted['input_sha256'], 'target inputs mismatch')
    by_instance = {row['instance']: row for row in accepted['rows']}
    counts, failures, missing, times, comparisons = Counter(), [], [], [], []
    for task, expected in enumerate(matrix):
        path = campaign / 'results' / f'{task:03d}.json'
        if not path.exists():
            missing.append(task)
            continue
        row = json.loads(path.read_text())
        require(row['task'] == task and str(row['rank']) == expected['rank'] and
                all(row[k] == expected[k] for k in ('arm', 'tier', 'instance', 'expected')), 'row identity mismatch')
        require(row['binary_sha256'] == manifest['arms'][expected['arm']]['sha256'], 'result binary mismatch')
        disposition, errors = score_row(row, by_instance[row['instance']], manifest['resource_contract'])
        counts[disposition] += 1
        if row.get('proof_check'):
            comparison = proof_cost_comparison(row['proof_check'],
                by_instance[row['instance']].get('proof_check'))
            comparison.update(task=task, instance=row['instance'])
            comparisons.append(comparison)
        if errors:
            failures.append(dict(task=task, instance=row['instance'], errors=errors))
        if not row.get('solve', {}).get('timed_out') and isinstance(row.get('solve', {}).get('wall_seconds'), (int, float)):
            times.append(row['solve']['wall_seconds'])
    total = math.fsum(times)
    ceiling = (manifest['acceptance_runtime_seconds'] if target_baseline is None
               else accepted['acceptance_runtime_seconds'])
    if not missing and not within_runtime_target(total, ceiling):
        failures.append(dict(errors=['aggregate solver-time ceiling exceeded']))
    require(len(list((campaign / 'results').glob('*.json'))) == len(matrix) - len(missing), 'unexpected result files')
    return dict(schema='tactix-controller-regression-score-v1', set=manifest['set'],
                manifest_sha256=sha(campaign / 'manifest.json'),
                scorer_sha256=sha(Path(__file__)),
                target=target_kind, target_baseline_path=score_target_path(baseline_path),
                target_baseline_sha256=sha(baseline_path),
                target_source_commit=baseline.get('target_source_commit'),
                runtime_tolerance_fraction=accepted.get('runtime_tolerance_fraction'),
                target_solver_seconds=accepted.get('paper_solver_seconds'),
                accepted=not missing and not failures, expected_rows=len(matrix),
                dispositions=dict(counts), missing=missing, failures=failures,
                solver_seconds=total, solver_seconds_ceiling=ceiling,
                independently_verified=counts['model-verified'] + counts['verified'],
                censored_proofs=counts['censored'],
                proof_cost_dispositions=dict(Counter(c['status'] for c in comparisons)),
                proof_cost_comparisons=comparisons,
                limitation='Bounded regression acceptance does not certify censored proofs.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('prepare')
    p.add_argument('--prefix', required=True)
    p.add_argument('--release', type=Path, required=True)
    p.add_argument('--dsr-transport', type=Path)
    p.add_argument('--solver-argument', action='append', default=[],
                   help='append a qualified Default affine-search or graph-labeling setting')
    s = commands.add_parser('score')
    s.add_argument('campaign', type=Path)
    s.add_argument('--output', type=Path)
    s.add_argument('--replace-output', action='store_true')
    pscore = commands.add_parser('score-paper')
    pscore.add_argument('campaign', type=Path)
    pscore.add_argument('--output', type=Path)
    pscore.add_argument('--replace-output', action='store_true')
    args = parser.parse_args()
    if args.command == 'prepare':
        require('/' not in args.prefix and args.prefix not in ('', '.', '..'), 'invalid prefix')
        print(json.dumps(prepare(args.prefix, args.release.resolve(), args.solver_argument, args.dsr_transport), indent=2))
    elif args.command == 'score':
        result = score(args.campaign.resolve())
        if args.output:
            write_score(args.output, result, args.replace_output)
        print(json.dumps(result, indent=2))
        return 0 if result['accepted'] else 1
    else:
        subprocess.run(['python3', str(BASELINE_FREEZER), '--verify'], check=True)
        result = score(args.campaign.resolve(), BASELINE)
        if args.output:
            write_score(args.output, result, args.replace_output)
        print(json.dumps(result, indent=2))
        return 0 if result['accepted'] else 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
