#!/usr/bin/env python3
"""Read-only fixed-44 coverage accounting; no qualification is inferred."""
import argparse
from collections import Counter
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import struct

from score_tierd_full import classify, require, sha

ROOT = Path(__file__).resolve().parents[2]
SELECTION = ROOT / 'benchmarks/reports/2026-10-07-fixed44-successor-campaign-preparation/selection.tsv'
SELECTION_SHA256 = '74939b00a3395aa21d108d4ab20eb0925f569c1ac2bb501939e01432551a3333'
RETENTION_RANKS = {147, 263, 630, 744}
CHECKER_EXECUTABLES = {
    'drat-trim': 'drat-trim',
    'cake_lpr': 'cake_lpr',
    'veripb': 'veripb',
    'cake-pb-cnf': 'cake-pb-cnf',
    'dsr-trim': 'dsr-trim',
    'dsr-compress': 'dsr-compress',
    'srcheck': 'srcheck',
}
PINNED_TOP_LEVEL = {'matrix.tsv', 'array.sbatch', 'validate.sbatch'}


def canonical_sha(value):
    wire = json.dumps(value, sort_keys=True, separators=(',', ':')).encode()
    return hashlib.sha256(wire).hexdigest()


def bound_json(campaign, name):
    path = campaign / name
    require(path.is_file() and not path.is_symlink(), 'missing binding: ' + name)
    return json.loads(path.read_text()), sha(path)


def parse_directive(path, name):
    matches = []
    prefix = '#SBATCH --' + name + '='
    for line in path.read_text().splitlines():
        if line.startswith(prefix):
            matches.append(line[len(prefix):])
    require(len(matches) <= 1, 'duplicate Slurm directive: ' + name)
    return matches[0] if matches else None


def parse_array(value):
    require(value is not None and '%' not in value, 'missing or throttled retry array')
    tasks = []
    for piece in value.split(','):
        require(bool(re.fullmatch(r'\d+(?:-\d+)?', piece)), 'invalid array task set')
        ends = [int(item) for item in piece.split('-')]
        start, stop = (ends[0], ends[0]) if len(ends) == 1 else ends
        require(start <= stop, 'reversed array task range')
        tasks.extend(range(start, stop + 1))
    require(len(set(tasks)) == len(tasks), 'duplicate array task')
    return tasks


def validate_pyc_supplement(campaign, manifest, supplement_path, undeclared):
    require(supplement_path is not None, 'undeclared pin file requires pyc supplement')
    supplement_path = supplement_path.resolve()
    require(supplement_path.is_file() and not supplement_path.is_symlink(),
            'missing pyc supplement')
    supplement = json.loads(supplement_path.read_text())
    require(supplement.get('schema') == 'tactix-validation-created-pyc-supplement-v1',
            'unsupported pyc supplement')
    require(isinstance(supplement.get('python_version'), str) and
            re.match(r'^3\.9\.', supplement['python_version']),
            'pyc supplement interpreter mismatch')
    require((ROOT / supplement['parent_campaign']).resolve() == campaign,
            'pyc supplement campaign mismatch')
    records = supplement.get('records')
    require(isinstance(records, list) and records, 'empty pyc supplement')
    by_name = {record.get('name'): record for record in records}
    require(None not in by_name and len(by_name) == len(records),
            'duplicate pyc supplement source')
    covered = set()
    for name, record in by_name.items():
        require(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', name) is not None,
                'invalid pyc supplement source')
        source_relative = f'pins/{name}.py'
        source = campaign / source_relative
        require(source_relative in manifest['pin_sha256'] and
                record.get('source_sha256') == manifest['pin_sha256'][source_relative] == sha(source),
                'pyc supplement source hash mismatch')
        candidates = sorted((campaign / 'pins/__pycache__').glob(f'{name}.*.pyc'))
        require(len(candidates) == 1, 'pyc supplement cache identity mismatch')
        pyc = candidates[0]
        require(not pyc.is_symlink(), 'pyc supplement cache is a symlink')
        relative = str(pyc.relative_to(campaign))
        require(relative in undeclared and record.get('pyc_sha256') == sha(pyc),
                'pyc supplement cache hash mismatch')
        raw = pyc.read_bytes()[:16]
        require(len(raw) == 16 and struct.unpack('<I', raw[4:8])[0] == 0,
                'unsupported pyc header')
        mtime, size = struct.unpack('<II', raw[8:16])
        require(record.get('source_mtime_header') == mtime and
                record.get('source_size_header') == size and
                mtime == (int(source.stat().st_mtime) & 0xFFFFFFFF) and
                size == source.stat().st_size and
                record.get('semantic_compile_match') is True,
                'pyc supplement semantic authority mismatch')
        covered.add(relative)
    require(covered == undeclared, 'pyc supplement does not cover undeclared pins')
    return dict(path=str(supplement_path), sha256=sha(supplement_path),
                schema=supplement['schema'], records=records,
                validation_job_id=supplement.get('validation_job_id'),
                python_version=supplement.get('python_version'))


def validate_pin_inventory(campaign, manifest, supplement_path):
    declared = set(manifest['pin_sha256'])
    require(PINNED_TOP_LEVEL <= declared, 'fixed44 top-level pin missing')
    require(all(relative in PINNED_TOP_LEVEL or relative.startswith('pins/')
                for relative in declared), 'unexpected pinned path')
    for relative, digest in manifest['pin_sha256'].items():
        path = campaign / relative
        resolved = path.resolve()
        require(resolved.is_relative_to(campaign) and path.is_file() and not path.is_symlink(),
                'pin outside campaign: ' + relative)
        require(sha(path) == digest, 'changed pin: ' + relative)
    actual = {str(path.relative_to(campaign))
              for path in (campaign / 'pins').rglob('*') if path.is_file()}
    declared_pins = {relative for relative in declared if relative.startswith('pins/')}
    require(declared_pins <= actual, 'declared pin missing from inventory')
    undeclared = actual - declared_pins
    if undeclared:
        return validate_pyc_supplement(
            campaign, manifest, supplement_path, undeclared)
    require(supplement_path is None, 'unused pyc supplement')
    return None


def validate_launch(campaign, manifest_hash):
    launch, launch_sha = bound_json(campaign, 'launch.json')
    validation_launch, validation_launch_sha = bound_json(campaign, 'validation-launch.json')
    validation, validation_sha = bound_json(campaign, 'validation.json')
    require(launch.get('manifest_sha256') == manifest_hash,
            'launch manifest binding mismatch')
    require(validation_launch.get('manifest_sha256') == manifest_hash,
            'validation manifest binding mismatch')
    require(validation.get('passed') is True and
            str(validation.get('job_id')) == str(validation_launch.get('job_id')),
            'campaign validation did not pass')
    for payload in (launch, validation_launch):
        require(re.fullmatch(r'\d+', str(payload.get('job_id', ''))) is not None,
                'invalid Slurm job binding')
    return dict(job_id=str(launch['job_id']), sha256=launch_sha,
                validation_job_id=str(validation_launch['job_id']),
                validation_launch_sha256=validation_launch_sha,
                validation_sha256=validation_sha)


def validate_checker_commands(campaign, status, record):
    if not status.startswith('checked_'):
        return
    if status == 'checked_SAT':
        checked = record['model_check']
        if 'command' in checked:
            command = checked['command']
            require(isinstance(command, list) and command and
                    command[0] == str(campaign / 'pins/checkmodel'),
                    'model checker executable mismatch')
        return
    checked = record['proof_check']
    for stage in checked['stages']:
        if 'command' not in stage:
            continue
        command = stage['command']
        executable = CHECKER_EXECUTABLES[stage['name']]
        require(isinstance(command, list) and command and
                command[0] == str(campaign / 'pins' / executable),
                'proof checker executable mismatch: ' + stage['name'])


def validate_retry_manifest(campaign, manifest, retry, allowed):
    array_script = campaign / 'array.sbatch'
    array_tasks = parse_array(parse_directive(array_script, 'array'))
    require(array_tasks == allowed, 'retry array differs from allowed tasks')
    exclude = parse_directive(array_script, 'exclude')
    if retry is None:
        require(allowed == list(range(44)) and exclude is None and
                manifest['resource_contract'].get('node_constraint') is None,
                'original attempt resource selection changed')
        return None
    require(retry.get('schema') == 'tactix-fixed44-retry-attempt-v1',
            'unsupported retry schema')
    resource = retry.get('resource_exclusion')
    constraint = manifest['resource_contract'].get('node_constraint')
    require(isinstance(resource, dict) and isinstance(constraint, dict) and
            resource.get('schema') == constraint.get('schema') ==
            'live-cooperative-scratch-holder-exclusion-v1',
            'retry resource exclusion schema mismatch')
    excluded = resource.get('excluded_nodes')
    require(isinstance(excluded, list) and excluded == sorted(set(excluded)) and excluded,
            'invalid retry excluded nodes')
    require(constraint.get('excluded_nodes') == excluded and
            exclude is not None and exclude.split(',') == excluded,
            'retry array exclusion mismatch')
    snapshot = (ROOT / resource['snapshot']).resolve()
    require(snapshot.is_relative_to(ROOT) and snapshot.is_file() and
            sha(snapshot) == resource.get('snapshot_sha256'),
            'retry holder snapshot mismatch')
    snapshot_rows = {}
    for line in snapshot.read_text().splitlines()[1:]:
        job, state, _elapsed, node, _reason = line.split('|')
        snapshot_rows[job] = (state, node)
    holders = resource.get('holders')
    require(isinstance(holders, list) and holders, 'retry holder evidence missing')
    require(sorted(holder['node'] for holder in holders) == excluded and
            len({holder['job_id'] for holder in holders}) == len(holders),
            'retry holder node coverage mismatch')
    for holder in holders:
        task = holder.get('task')
        require(type(task) is int and task not in allowed and
                snapshot_rows.get(holder.get('job_id')) == ('RUNNING', holder.get('node')),
                'retry holder snapshot entry mismatch')
        claim = (ROOT / holder['claim']).resolve()
        require(claim.is_relative_to(ROOT) and claim.is_file() and
                sha(claim) == holder.get('claim_sha256'),
                'retry holder claim hash mismatch')
        payload = json.loads(claim.read_text())
        require(payload.get('task') == task and payload.get('node') == holder.get('node'),
                'retry holder claim mismatch')
    return dict(array_tasks=array_tasks, excluded_nodes=excluded,
                snapshot_sha256=resource['snapshot_sha256'])


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
                          record_node=(record.get('node') if record else None),
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


def load(campaign, pyc_supplement=None):
    campaign = campaign.resolve()
    require(sha(SELECTION) == SELECTION_SHA256, 'fixed44 selection changed')
    manifest_path = campaign / 'manifest.json'
    manifest = json.loads(manifest_path.read_text())
    manifest_hash = sha(manifest_path)
    launch = validate_launch(campaign, manifest_hash)
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
    pyc_authority = validate_pin_inventory(campaign, manifest, pyc_supplement)
    if pyc_authority is not None:
        require(str(pyc_authority.get('validation_job_id')) ==
                launch['validation_job_id'], 'pyc supplement validation job mismatch')
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
    allowed = retry['allowed_tasks'] if retry else list(range(44))
    require(all(type(t) is int for t in allowed) and
            allowed == sorted(set(allowed)) and set(allowed) <= set(range(44)),
            'invalid allowed retry task')
    retry_resources = validate_retry_manifest(campaign, manifest, retry, allowed)
    allowed_set = set(allowed)
    for path in sorted((campaign / 'results').glob('*.json')):
        record = json.loads(path.read_text())
        task = record['task']
        require(type(task) is int and path.name == f'{task:03d}.json', 'result filename mismatch')
        require(task not in records, 'duplicate task')
        require(task in allowed_set, 'result outside declared retry tasks')
        if record.get('solve') is not None:
            require(record.get('binary_sha256') == arm['sha256'], 'result binary mismatch')
            command = record.get('command', [])
            require(command.count(str(binary)) == 1, 'result binary command mismatch')
            pos = command.index(str(binary)) + 1
            require(command[pos:pos + len(arm['arguments'])] == arm['arguments'] and
                    len(command) == pos + len(arm['arguments']) + 2,
                    'result arguments mismatch')
        status = checked_classification(matrix[task], record)[0]
        validate_checker_commands(campaign, status, record)
        records[task] = record
        hashes[str(path.relative_to(campaign))] = sha(path)
    result = summarize(selection, matrix, records)
    result.update(schema='tactix-fixed44-coverage-v1', campaign=str(campaign),
                  manifest_sha256=manifest_hash,
                  scorer_sha256=sha(Path(__file__)), candidate=arm,
                  selection_sha256=SELECTION_SHA256, record_sha256=hashes,
                  retry_attempt=retry,
                  retry_metadata_sha256=(canonical_sha(retry) if retry else None),
                  retry_resources=retry_resources,
                  launch=launch, pyc_supplement=pyc_authority,
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
    for attempt, report in enumerate(reports[1:], 1):
        require(report['campaign'] not in seen, 'duplicate attempt campaign')
        require(report['coherence'] == base['coherence'], 'retry changes coherent candidate or oracle')
        retry = report['retry_attempt']
        require(retry and retry.get('schema') == 'tactix-fixed44-retry-attempt-v1',
                'undeclared retry attempt')
        require(retry.get('attempt') == attempt and
                report.get('retry_metadata_sha256') == canonical_sha(retry),
                'retry attempt order or metadata mismatch')
        parent = reports[attempt - 1]
        parent_path = str((ROOT / retry['parent']['campaign']).resolve())
        require(parent_path == parent['campaign'] and
                parent['manifest_sha256'] == retry['parent']['manifest_sha256'] and
                parent['launch']['job_id'] == str(retry['parent'].get('launch_job_id')) and
                parent['launch']['sha256'] == retry['parent'].get('launch_sha256'),
                'retry parent binding mismatch')
        launch_campaigns = {candidate['launch']['job_id']: candidate['campaign']
                            for candidate in reports[:attempt]}
        for holder in retry['resource_exclusion']['holders']:
            job = holder['job_id'].split('_', 1)[0]
            require(job in launch_campaigns and
                    (ROOT / holder['claim']).resolve() ==
                    Path(launch_campaigns[job]) / 'claims' / f'{holder["task"]:03d}.json',
                    'retry holder launch binding mismatch')
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
            require(predecessor.get('coverage_rank') == old['task'] + 1 and
                    predecessor.get('instance') == old['instance'] and
                    predecessor.get('node') == old.get('record_node'),
                    'retry predecessor identity mismatch')
            if replacement['status'] != 'missing':
                cells[task] = dict(replacement)
        seen[report['campaign']] = report
    result = summarize_cells(cells)
    result.update(schema='tactix-fixed44-merged-coverage-v1',
                  scorer_sha256=sha(Path(__file__)), candidate=base['candidate'],
                  selection_sha256=SELECTION_SHA256,
                  attempts=[{k: r[k] for k in ('campaign', 'manifest_sha256', 'record_sha256',
                                               'retry_attempt', 'launch', 'pyc_supplement')}
                            for r in reports],
                  trusted_runner_boundary=('Checker command identity is verified when the pinned '
                    'runtime recorded a command. Legacy records without checker commands rely on '
                    'the hash-bound runtime and checker pins.'))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pyc-supplement', type=Path, action='append', default=[],
                        help='explicit hash-bound authority for validation-created pycs; repeatable')
    parser.add_argument('campaign', type=Path, nargs='+', help='original campaign, then immutable retries')
    args = parser.parse_args()
    supplements = {}
    for path in args.pyc_supplement:
        payload = json.loads(path.read_text())
        require(payload.get('schema') == 'tactix-validation-created-pyc-supplement-v1',
                'unsupported pyc supplement')
        campaign = (ROOT / payload['parent_campaign']).resolve()
        require(campaign not in supplements, 'duplicate pyc supplement campaign')
        supplements[campaign] = path
    reports = [load(c, supplements.pop(c.resolve(), None)) for c in args.campaign]
    require(not supplements, 'unused pyc supplement campaign')
    print(json.dumps(merge(reports), indent=2))


if __name__ == '__main__':
    main()
