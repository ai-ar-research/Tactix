#!/usr/bin/env python3
"""Score the five pinned Tier D arms after their full arrays terminate."""
from collections import Counter
import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ARCHIVE = ROOT / 'benchmarks/baselines/sc24-26-32g-5000s/manifest.json'
CAMPAIGNS = {
    'kissat-sc2024': '20260928-am540-four-winner-tierd-cpuq',
    'lymphosat': '20260928-am540-four-winner-tierd-cpuq',
    'ae-kissat-mab': '20260928-am540-four-winner-tierd-cpuq',
    'satsuma-kissat': '20260928-am547-satsuma-hybrid-tierd-cpuq',
    'tactix': '20260928-am546-no-mab-tactix-hybrid-tierd-cpuq',
}
ROUTES = {
    'kissat-sc2024': {('drat-trim', 'cake-lpr')},
    'lymphosat': {('veripb', 'cake-pb-cnf')},
    'ae-kissat-mab': {('drat-trim', 'cake-lpr')},
    'satsuma-kissat': {('drat-trim', 'cake-lpr'),
                       ('dsr-trim', 'dsr-compress', 'srcheck')},
    'tactix': {('veripb', 'cake-pb-cnf'), ('drat-trim', 'cake_lpr'),
               ('dsr-trim', 'dsr-compress', 'srcheck')},
}
ARM_ORDER = tuple(CAMPAIGNS)
SCRATCH_INCOMPLETE_REASONS = {
    'scratch_admission', 'scratch_file_limit',
    'scratch_file_limit_unavailable', 'scratch_handle_limit',
    'scratch_node_busy', 'scratch_enospc',
}


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def par2_total(times, resource_incomplete):
    return None if resource_incomplete else round(math.fsum(times), 3)


def read_campaign(name):
    path = ROOT / 'benchmarks/runs' / name
    manifest = json.loads((path / 'manifest.json').read_text())
    launch = json.loads((path / 'launch.json').read_text())
    require(launch['manifest_sha256'] == sha(path / 'manifest.json'),
            name + ': launch manifest changed')
    binding = manifest.get('baseline_authority', manifest.get('dataset_authority'))
    require(binding and binding['sha256'] == sha(ARCHIVE),
            name + ': archived dataset authority changed')
    for rel, digest in manifest['pin_sha256'].items():
        require(sha(path / rel) == digest, name + ': pin changed: ' + rel)
    contract = manifest['resource_contract']
    for key, value in (('cpus_per_task', 4), ('memory_mib', 32768),
                       ('solve_timeout', 5000), ('checker_timeout', 14400)):
        require(contract[key] == value, name + ': resource contract ' + key)
    if name.endswith('tactix-hybrid-tierd-cpuq'):
        with (path / 'matrix.tsv').open(newline='') as stream:
            matrix = list(csv.DictReader(stream, delimiter='\t'))
    else:
        matrix = manifest['matrix']
    require([int(row['task']) for row in matrix] == list(range(len(matrix))),
            name + ': matrix task order')
    return path, manifest, matrix


def classify(row, result, arm):
    if result is None:
        return 'missing', None, None
    for field in ('arm', 'instance', 'expected'):
        require(result[field] == row[field], arm + ': result identity ' + field)
    require(int(result['rank']) == int(row['rank']) and
            int(result['task']) == int(row['task']), arm + ': rank/task identity')
    completion = result.get('completion')
    if isinstance(completion, dict) and completion.get('status') == 'incomplete':
        require(completion.get('reason') in SCRATCH_INCOMPLETE_REASONS,
                arm + ': unsupported incomplete resource reason')
        return 'resource_incomplete', None, completion.get('reason')
    clean = (result.get('artifact_deleted') is True and
             result.get('derived_artifacts_deleted') is True and
             result.get('cleanup_pending') is False)
    if not clean:
        return 'residue', None, None
    solve = result.get('solve')
    if not isinstance(solve, dict):
        return 'harness_failure', None, None
    seconds = solve.get('wall_seconds')
    require(isinstance(seconds, (int, float)) and math.isfinite(seconds) and
            0 <= seconds <= 5002, arm + ': invalid solve timing')
    if solve.get('timed_out'):
        if (result.get('verdict') == 'UNKNOWN' and result.get('proof_check') is None
                and result.get('model_check') is None):
            return 'solver_timeout', seconds, None
        return 'solver_timeout_with_verdict', seconds, None
    expected = row['expected']
    verdict = result.get('verdict')
    if (verdict not in ('SAT', 'UNSAT') or
            expected not in (verdict, 'UNKNOWN') or
            result.get('expected_match') is not True):
        return 'verdict_mismatch', seconds, None
    if solve.get('exit_code') != {'SAT': 10, 'UNSAT': 20}[verdict]:
        return 'solver_abnormal_exit', seconds, None
    checked = result.get('model_check' if verdict == 'SAT' else 'proof_check')
    if not isinstance(checked, dict):
        return 'missing_checker_result', seconds, None
    if checked.get('timed_out'):
        return 'checker_timeout', seconds, None
    if verdict == 'UNSAT':
        stages = checked.get('stages') or []
        names = tuple(stage.get('name') for stage in stages)
        if names not in ROUTES[arm] or not all(
                stage.get('result') == 'verified' for stage in stages):
            return 'proof_route_or_stage_failure', seconds, names
    if checked.get('result') != 'verified' or result.get('valid') is not True:
        return 'certificate_rejected', seconds, None
    return 'checked_' + verdict, seconds, None


def score_arm(arm, archive):
    path, manifest, matrix = read_campaign(CAMPAIGNS[arm])
    rows = [row for row in matrix if row['arm'] == arm]
    require(len(rows) == 1166, arm + ': incomplete matrix')
    by_rank = {int(row['rank']): row for row in rows}
    require(set(by_rank) == set(range(1, 1167)), arm + ': rank coverage')
    expected = archive['instances']
    for rank, row in by_rank.items():
        reference = expected[rank - 1]
        require((row['instance'], row['expected']) ==
                (reference['rel'], reference['expected']),
                arm + ': dataset differs at rank ' + str(rank))
    counts = Counter()
    raw_times = []
    verified_times = []
    raw_solves = 0
    unusual = []
    for rank in range(1, 1167):
        row = by_rank[rank]
        task = int(row['task'])
        result_path = path / 'results' / f'{task:05d}.json' if arm != 'tactix' else path / 'results' / f'{task:03d}.json'
        result = json.loads(result_path.read_text()) if result_path.exists() else None
        if result is not None and result.get('solve') is not None:
            binary = path / manifest['arms'][arm]['path']
            command = result.get('command', [])
            adapter = (str(path / 'pins/portfolio_rerun.py') in command and
                       str(path) in command and arm in command and
                       ('adapter-lymphosat' if arm == 'lymphosat' else
                        'adapter-satsuma') in command)
            require(str(binary) in command or adapter,
                    arm + ': solver command identity ' + str(rank))
            if arm == 'tactix':
                require(result['binary_sha256'] == manifest['arms'][arm]['sha256'],
                        arm + ': binary identity ' + str(rank))
        status, seconds, detail = classify(row, result, arm)
        counts[status] += 1
        raw_solved = status in ('checked_SAT', 'checked_UNSAT',
                                'checker_timeout', 'proof_route_or_stage_failure',
                                'certificate_rejected', 'missing_checker_result')
        raw_solves += raw_solved
        if status != 'resource_incomplete':
            raw_times.append(seconds if raw_solved and seconds is not None else 10000)
            verified_times.append(seconds if status.startswith('checked_') else 10000)
        if status not in ('checked_SAT', 'checked_UNSAT', 'solver_timeout'):
            unusual.append(dict(rank=rank, task=task, status=status, detail=detail))
    return dict(campaign=str(path.relative_to(ROOT)),
                manifest_sha256=sha(path / 'manifest.json'),
                final_records=1166-counts['missing'],
                terminal_complete=counts['missing'] == 0,
                eligible_for_comparison=not any(counts[key] for key in
                    ('missing', 'harness_failure', 'residue',
                     'solver_timeout_with_verdict', 'resource_incomplete')),
                classes=dict(counts),
                raw_solver_solved=raw_solves,
                checked=counts['checked_SAT']+counts['checked_UNSAT'],
                par2_censored_records=counts['resource_incomplete'],
                raw_solver_par2=par2_total(
                    raw_times, counts['resource_incomplete']),
                verified_par2=par2_total(
                    verified_times, counts['resource_incomplete']),
                unusual=unusual)


def main():
    archive = json.loads(ARCHIVE.read_text())
    require(len(archive['instances']) == 1166, 'archive size')
    arms = {arm: score_arm(arm, archive) for arm in ARM_ORDER}
    print(json.dumps(dict(schema='tactix-tierd-five-arm-score-v1',
                          archive_manifest_sha256=sha(ARCHIVE), arms=arms), indent=2))


if __name__ == '__main__':
    main()
