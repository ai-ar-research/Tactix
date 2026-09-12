#!/usr/bin/env python3
"""Offline, fail-closed winner-VBS coverage selection; never runs a solver.

Existing 5000-second baseline records are censored at the requested budget.
The latest Tactix campaign must supply actual solver timeout records; missing
tasks, OOM, rejected certificates, and pending checks are never timeouts.
"""
import argparse
from collections import Counter, defaultdict
import csv
import datetime as dt
import hashlib
import json
import math
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value):
    return type(value) in (int, float) and math.isfinite(value) and value >= 0


def clean(record):
    return (record.get('artifact_deleted') is True and
            record.get('derived_artifacts_deleted') is True and
            record.get('cleanup_pending') is False)


def baseline_eligible(record, seconds, memory_kib):
    verdict = record.get('verdict')
    solve = record.get('solve') or {}
    if verdict not in ('SAT', 'UNSAT'):
        return False
    checker = record.get('model_check' if verdict == 'SAT' else 'proof_check') or {}
    oracle = record.get('model_ok' if verdict == 'SAT' else 'proof_ok') is True
    if verdict == 'SAT':
        oracle = oracle and checker.get('exit_code') == 0 and not checker.get('timed_out')
    else:
        stages = checker.get('stages') or []
        oracle = oracle and bool(stages) and all(
            s.get('exit_code') == 0 and s.get('result') == 'verified' and
            s.get('timed_out') is False for s in stages)
    return bool(record.get('valid') is True and clean(record) and oracle and
                checker.get('result') == 'verified' and
                record.get('expected') in (verdict, 'UNKNOWN') and
                record.get('expected_match') is True and
                record.get('vbs_eligible') in ('SAT+UNSAT', 'SAT-only') and
                not (record.get('vbs_eligible') == 'SAT-only' and verdict != 'SAT') and
                not (record.get('solver') == 'lymphosat' and verdict != 'SAT') and
                solve.get('timed_out') is False and
                solve.get('exit_code') == {'SAT': 10, 'UNSAT': 20}[verdict] and
                number(solve.get('wall_seconds')) and solve['wall_seconds'] <= seconds and
                number(solve.get('peak_rss_kb')) and solve['peak_rss_kb'] <= memory_kib)


def genuine_timeout(record, final, seconds, memory_kib):
    solve = record.get('solve') or {}
    return bool(final and clean(record) and solve.get('timed_out') is True and
                number(solve.get('wall_seconds')) and solve['wall_seconds'] >= seconds and
                number(solve.get('peak_rss_kb')) and solve['peak_rss_kb'] <= memory_kib and
                record.get('verdict') == 'UNKNOWN' and
                solve.get('exit_code') not in (10, 20) and
                not record.get('proof_check') and not record.get('model_check'))


def order_key(candidate):
    solvers = candidate['solvers']
    lympho_only = candidate['verdict'] == 'SAT' and solvers == ['lymphosat']
    return (not lympho_only, len(solvers), candidate['fastest_seconds'], candidate['instance'])


def select(candidates, per_status):
    selected = []
    for verdict in ('SAT', 'UNSAT'):
        selected.extend(sorted((r for r in candidates if r['verdict'] == verdict),
                               key=order_key)[:per_status])
    return selected


def snapshot(baseline, tactix, seconds, memory_kib, per_status):
    evidence = {str(Path(__file__)): digest(Path(__file__))}
    def read(path):
        raw = path.read_bytes()
        evidence[str(path)] = hashlib.sha256(raw).hexdigest()
        return json.loads(raw)
    bm = read(baseline / 'manifest.json')
    br = read(baseline / 'results.json')
    bv = read(baseline / 'validation.json')
    authority = evidence[str(baseline / 'manifest.json')]
    require(br['manifest_sha256'] == bv['manifest_sha256'] == authority,
            'baseline aggregate/validation manifest mismatch')
    require(br.get('complete') is True and bv.get('passed') is True,
            'baseline campaign or validation incomplete')
    require(br.get('cleanup_pending') == 0 and not br.get('cleanup_contract_failures'),
            'baseline cleanup unresolved')
    require(bm['limits']['solve_wall_seconds'] >= seconds, 'baseline budget too short')
    require(bm['limits'].get('scheduler_memory_gib', bm['limits'].get('process_memory_gib'))
            * 1024 * 1024 == memory_kib, 'baseline memory contract differs')
    require(len(br['results']) == len(bm['tasks']), 'baseline task count mismatch')
    baseline_inputs = {r['rel']: r['expected'] for r in bm['instances']}
    winners = defaultdict(list)
    seen = set()
    for r in br['results']:
        task = r['task']
        require(type(task) is int and 0 <= task < len(bm['tasks']) and task not in seen,
                'invalid or duplicate baseline task')
        seen.add(task)
        row = bm['tasks'][task]
        require((r['instance'], r['expected'], r['solver']) ==
                (bm['instances'][row['instance']]['rel'],
                 bm['instances'][row['instance']]['expected'],
                 bm['solvers'][row['solver']]['name']), 'baseline identity mismatch')
        if baseline_eligible(r, seconds, memory_kib):
            winners[r['instance']].append(r)
    tm = read(tactix / 'manifest.json')
    require(tm['dataset_authority']['sha256'] == authority,
            'latest Tactix does not reference this baseline dataset authority')
    contract = tm['resource_contract']
    require(contract['solve_timeout'] == seconds and contract['memory_mib'] * 1024 == memory_kib,
            'Tactix budget mismatch')
    matrix_path = tactix / 'matrix.tsv'
    evidence[str(matrix_path)] = digest(matrix_path)
    require(tm['pin_sha256']['matrix.tsv'] == evidence[str(matrix_path)], 'matrix hash mismatch')
    rows = list(csv.DictReader(matrix_path.open(), delimiter='\t'))
    require({r['instance']: r['expected'] for r in rows} == baseline_inputs,
            'campaign dataset mismatch')
    require(len(rows) == len(baseline_inputs), 'duplicate Tactix instance')
    classes = Counter()
    candidates = []
    for index, row in enumerate(rows):
        require(int(row['task']) == index and row['arm'] == 'tactix', 'Tactix matrix identity')
        final_path = tactix / 'results' / f'{index:03d}.json'
        path = final_path if final_path.exists() else tactix / 'provisional' / f'{index:03d}.json'
        if not path.exists():
            classes['solver_pending_or_missing'] += 1
            continue
        r = read(path)
        require((r['task'], r['instance'], r['expected'], r['arm']) ==
                (index, row['instance'], row['expected'], 'tactix'), 'Tactix result identity')
        # Provisional records may omit the binary hash; final records may not.
        if path == final_path and r.get('solve'):
            require(r['binary_sha256'] == tm['arms']['tactix']['sha256'], 'Tactix binary mismatch')
        timeout = genuine_timeout(r, path == final_path, seconds, memory_kib)
        unresolved_timeout = path != final_path and r.get('solve', {}).get('timed_out') is True
        classes['genuine_timeout' if timeout else 'timeout_cleanup_pending' if unresolved_timeout
                else 'other_published_solver_outcome'] += 1
        if not timeout or row['instance'] not in winners:
            continue
        ws = winners[row['instance']]
        require(len({w['verdict'] for w in ws}) == 1, 'conflicting checked baseline verdicts')
        fastest = min(ws, key=lambda w: (w['solve']['wall_seconds'], w['solver']))
        require(row['expected'] in ('UNKNOWN', fastest['verdict']), 'timeout label mismatch')
        candidates.append(dict(instance=row['instance'], verdict=fastest['verdict'],
            solvers=sorted(w['solver'] for w in ws), fastest_solver=fastest['solver'],
            fastest_seconds=fastest['solve']['wall_seconds'],
            baseline_tasks=[w['task'] for w in ws], tactix_task=index,
            tactix_record=str(path), tactix_seconds=r['solve']['wall_seconds'],
            baseline_peak_rss_kib=fastest['solve']['peak_rss_kb']))
    selected = select(candidates, per_status)
    enough = all(sum(r['verdict'] == v for r in selected) == per_status for v in ('SAT', 'UNSAT'))
    complete = classes['solver_pending_or_missing'] == classes['timeout_cleanup_pending'] == 0
    return dict(schema='tactix-winner-coverage-selection-v1',
        observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), evidence_sha256=evidence,
        seconds=seconds, memory_kib=memory_kib, per_status=per_status,
        tactix_classes=dict(classes), baseline_covered_instances=len(winners),
        baseline_lymphosat_only_sat=sum(len(ws) == 1 and ws[0]['solver'] == 'lymphosat'
            and ws[0]['verdict'] == 'SAT' for ws in winners.values()),
        candidate_counts=dict(Counter(r['verdict'] for r in candidates)),
        selection_final=bool(enough and complete), selected=selected,
        candidates=sorted(candidates, key=lambda r: (r['verdict'], order_key(r))),
        policy='10 SAT and10 UNSAT by default. SAT LymphoSAT-only first; then fewer qualifying '
               'baselines, fastest validated baseline time, lexical identity as deterministic tie-break. '
               'No identity enters solver policy. Final selection requires complete solver-phase coverage; '
               'pending certificate checks do not block selection or independent development. '
               'No family balancing silently overrides the requested rarity order.',
        caveats='Cross-hardware solve/timeout signal only, not a timing ratio. Baseline32GiB '
                'contract plus recorded solver peak RSS is not reconstruction of simultaneous '
                'process-tree memory. Missing/OOM/checker failures are not selected as timeouts. '
                'Matched hwu reruns of Tactix and only the fastest qualifying baseline are required.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--tactix', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=1200)
    parser.add_argument('--memory-gib', type=int, default=32)
    parser.add_argument('--per-status', type=int, default=10)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-final', action='store_true')
    args = parser.parse_args()
    require(number(args.seconds) and args.seconds > 0 and args.memory_gib > 0 and args.per_status > 0,
            'invalid selection limits')
    result = snapshot(args.baseline, args.tactix, args.seconds,
                      args.memory_gib * 1024 * 1024, args.per_status)
    if args.require_final:
        require(result['selection_final'], 'latest solver phase or requested quota incomplete')
    with args.output.open('x') as f:
        json.dump(result, f, indent=2)
        f.write('\n')
    print(json.dumps({k: result[k] for k in ('selection_final', 'tactix_classes',
          'baseline_covered_instances', 'baseline_lymphosat_only_sat', 'candidate_counts')}, indent=2))


if __name__ == '__main__':
    main()
