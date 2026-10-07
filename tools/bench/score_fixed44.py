#!/usr/bin/env python3
"""Read-only fixed-44 coverage accounting; no qualification is inferred."""
import argparse
from collections import Counter
import csv
import json
import math
from pathlib import Path

from score_tierd_full import classify, require, sha

ROOT = Path(__file__).resolve().parents[2]
SELECTION = ROOT / 'benchmarks/reports/2026-10-07-fixed44-successor-campaign-preparation/selection.tsv'
SELECTION_SHA256 = '74939b00a3395aa21d108d4ab20eb0925f569c1ac2bb501939e01432551a3333'
RETENTION_RANKS = {147, 263, 630, 744}


def checked_classification(row, record):
    """A resource censor wins over contradictory claimed verification."""
    status, seconds, detail = classify(row, record, 'tactix')
    if status.startswith('checked_'):
        checker = record['model_check' if status == 'checked_SAT' else 'proof_check']
        if checker.get('exit_code') != 0 or checker.get('timed_out') is not False:
            return 'inconsistent_checker_result', seconds, None
        elapsed = checker.get('wall_seconds')
        if (type(elapsed) not in (int, float) or not math.isfinite(elapsed)
                or not 0 <= elapsed <= 14400):
            return 'checker_time_outside_contract', seconds, None
        if status == 'checked_UNSAT':
            if any(s.get('exit_code') != 0 or s.get('timed_out') is not False
                   for s in checker['stages']):
                return 'inconsistent_checker_stage', seconds, None
    return status, seconds, detail


def summarize(selection, matrix, records):
    require(len(selection) == len(matrix) == 44, 'fixed44 denominator changed')
    require([int(r['coverage_rank']) for r in selection] == list(range(1, 45)),
            'selection rank order')
    require(len({r['instance'] for r in selection}) == 44, 'duplicate selection')
    require(len({int(r['original_tierd_rank']) for r in selection}) == 44,
            'duplicate original rank')
    require(set(records) <= set(range(44)), 'unexpected result task')
    cells = []
    for task, (selected, row) in enumerate(zip(selection, matrix)):
        require(int(row['task']) == task and int(row['rank']) == task + 1,
                'matrix task/rank order')
        require(row['arm'] == 'tactix' and row['instance'] == selected['instance']
                and row['expected'] == selected['verdict'], 'matrix selection mismatch')
        record = records.get(task)
        status, seconds, detail = checked_classification(row, record)
        original = int(selected['original_tierd_rank'])
        cells.append(dict(task=task, original_rank=original, instance=row['instance'],
                          status=status, solver_seconds=seconds, detail=detail,
                          retryable=bool(record and
                                         record.get('completion', {}).get('retryable') is True)))
    return summarize_cells(cells)


def summarize_cells(cells):
    require(len(cells) == 44, 'fixed44 denominator changed')
    counts = Counter(c['status'] for c in cells)
    verified = [c['original_rank'] for c in cells if c['status'].startswith('checked_')]
    comparable = all(c['status'] in ('checked_SAT', 'checked_UNSAT', 'solver_timeout')
                     for c in cells)
    return dict(expected_instances=44, final_records=44 - counts['missing'], dispositions=dict(counts),
                independently_verified=len(verified), verified_original_ranks=verified,
                twelve_verified_results=len(verified) >= 12,
                retained_original_ranks=sorted(RETENTION_RANKS.intersection(verified)),
                missing_retention_ranks=sorted(RETENTION_RANKS.difference(verified)),
                all_rows_comparable=comparable,
                par2_seconds=(sum(c['solver_seconds'] if c['status'].startswith('checked_')
                                  else 10000 for c in cells) if comparable else None),
                rows=cells,
                qualification_accepted=False,
                limitation='Coverage only. All 44 rows remain in the denominator. Missing, '
                'resource-censored, rejected, unclean, and checker-censored results do not '
                'close instances and suppress complete-arm PAR-2. Twelve verified results '
                'do not establish N1-N4, fuzz, Debug, or regression acceptance.')


def load(campaign):
    campaign = campaign.resolve()
    require(sha(SELECTION) == SELECTION_SHA256, 'fixed44 selection changed')
    manifest = json.loads((campaign / 'manifest.json').read_text())
    require(manifest['selection_authority']['sha256'] == SELECTION_SHA256,
            'campaign selection authority mismatch')
    require(set(manifest['arms']) == {'tactix'}, 'not a single candidate')
    arm = manifest['arms']['tactix']
    require(arm.get('certificate') == 'auto' and
            arm.get('arguments', [])[:1] == ['--auto-proof'], 'automatic proof required')
    contract = manifest['resource_contract']
    for key, value in (('solve_timeout', 5000), ('checker_timeout', 14400),
                       ('cpus_per_task', 4), ('memory_mib', 32768)):
        require(contract[key] == value, 'fixed44 resource contract: ' + key)
    for relative, digest in manifest['pin_sha256'].items():
        path = (campaign / relative).resolve()
        require(path.is_relative_to(campaign), 'pin outside campaign')
        require(sha(path) == digest, 'changed pin: ' + relative)
    binary = (campaign / arm['path']).resolve()
    require(binary.is_relative_to(campaign) and sha(binary) == arm['sha256'],
            'candidate binary mismatch')
    with SELECTION.open(newline='') as stream:
        selection = list(csv.DictReader(stream, delimiter='\t'))
    with (campaign / 'matrix.tsv').open(newline='') as stream:
        matrix = list(csv.DictReader(stream, delimiter='\t'))
    require(set(manifest['input_sha256']) == {r['instance'] for r in selection},
            'input hash coverage mismatch')
    records, hashes = {}, {}
    retry = manifest.get('retry_attempt')
    allowed = set(retry['allowed_tasks']) if retry else set(range(44))
    require(all(type(t) is int for t in allowed) and allowed <= set(range(44)),
            'invalid allowed retry task')
    for path in sorted((campaign / 'results').glob('*.json')):
        record = json.loads(path.read_text())
        task = record['task']
        require(type(task) is int and path.name == f'{task:03d}.json', 'result filename mismatch')
        require(task not in records, 'duplicate task')
        require(task in allowed, 'result outside declared retry tasks')
        if record.get('solve') is not None:
            require(record.get('binary_sha256') == arm['sha256'], 'result binary mismatch')
            command = record.get('command', [])
            require(command.count(str(binary)) == 1, 'result binary command mismatch')
            pos = command.index(str(binary)) + 1
            require(command[pos:pos + len(arm['arguments'])] == arm['arguments'] and
                    len(command) == pos + len(arm['arguments']) + 2,
                    'result arguments mismatch')
        records[task] = record
        hashes[str(path.relative_to(campaign))] = sha(path)
    result = summarize(selection, matrix, records)
    result.update(schema='tactix-fixed44-coverage-v1', campaign=str(campaign),
                  manifest_sha256=sha(campaign / 'manifest.json'),
                  scorer_sha256=sha(Path(__file__)), candidate=arm,
                  selection_sha256=SELECTION_SHA256, record_sha256=hashes,
                  retry_attempt=retry,
                  coherence=dict(candidate={k: arm[k] for k in ('sha256', 'arguments', 'certificate')},
                                 source_commit=manifest['source_commit'],
                                 input_sha256=manifest['input_sha256'],
                                 dsr_checker_profile=manifest.get('dsr_checker_profile'),
                                 pins={k: v for k, v in manifest['pin_sha256'].items()
                                       if k.startswith('pins/')}))
    for cell in result['rows']:
        cell['source_campaign'] = str(campaign)
    return result


def merge(reports):
    """Merge explicitly authorized retries without replacing their evidence."""
    require(bool(reports) and reports[0]['retry_attempt'] is None,
            'first campaign must be the original full44 attempt')
    base = reports[0]
    cells = [dict(c) for c in base['rows']]
    seen = {base['campaign']: base}
    for report in reports[1:]:
        require(report['campaign'] not in seen, 'duplicate attempt campaign')
        require(report['coherence'] == base['coherence'], 'retry changes coherent candidate or oracle')
        retry = report['retry_attempt']
        require(retry and retry.get('schema') == 'tactix-fixed44-retry-attempt-v1',
                'undeclared retry attempt')
        parent_path = str((ROOT / retry['parent']['campaign']).resolve())
        require(parent_path in seen and
                seen[parent_path]['manifest_sha256'] == retry['parent']['manifest_sha256'],
                'retry parent binding mismatch')
        allowed = retry['allowed_tasks']
        require(len(set(allowed)) == len(allowed), 'duplicate allowed retry task')
        predecessors = {p['task']: p for p in retry['predecessors']}
        require(len(predecessors) == len(retry['predecessors']) and
                set(predecessors) == set(allowed), 'retry predecessor coverage mismatch')
        for task in allowed:
            old, replacement, predecessor = cells[task], report['rows'][task], predecessors[task]
            require(old['status'] == 'resource_incomplete' and old['detail'] == 'scratch_node_busy'
                    and old['retryable'], 'retry of non-retryable result')
            source = seen[old['source_campaign']]
            relative = f'results/{task:03d}.json'
            require((ROOT / predecessor['result']).resolve() == Path(source['campaign']) / relative
                    and predecessor['result_sha256'] == source['record_sha256'][relative],
                    'retry predecessor result binding mismatch')
            require(predecessor.get('completion_status') == 'incomplete' and
                    predecessor.get('completion_reason') == 'scratch_node_busy' and
                    predecessor.get('retryable') is True, 'retry predecessor disposition mismatch')
            if replacement['status'] != 'missing':
                cells[task] = dict(replacement)
        seen[report['campaign']] = report
    result = summarize_cells(cells)
    result.update(schema='tactix-fixed44-merged-coverage-v1',
                  scorer_sha256=sha(Path(__file__)), candidate=base['candidate'],
                  selection_sha256=SELECTION_SHA256,
                  attempts=[{k: r[k] for k in ('campaign', 'manifest_sha256', 'record_sha256',
                                               'retry_attempt')} for r in reports])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('campaign', type=Path, nargs='+', help='original campaign, then immutable retries')
    args = parser.parse_args()
    reports = [load(c) for c in args.campaign]
    print(json.dumps(merge(reports), indent=2))


if __name__ == '__main__':
    main()
