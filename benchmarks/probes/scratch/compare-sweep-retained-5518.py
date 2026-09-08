#!/usr/bin/env python3
"""Validate repaired observer output and localize, without assuming closure."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import itertools
import json

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5515-sweep-retained-rank32-trace'
def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            digest.update(block)
    return digest.hexdigest()

def first_difference(a, b):
    for index, (x, y) in enumerate(itertools.zip_longest(a, b)):
        if x != y:
            return dict(index=index, candidate=x, comparison=y)
    return None

def compare_streams(a, b):
    first, counts = None, [0, 0]
    with a.open() as x, b.open() as y:
        for index, pair in enumerate(itertools.zip_longest(x, y)):
            for side, row in enumerate(pair):
                counts[side] += row is not None
            if first is None and pair[0] != pair[1]:
                first = dict(index=index, candidate=pair[0].strip().split('\t') if pair[0] else None,
                             comparison=pair[1].strip().split('\t') if pair[1] else None)
    return dict(counts=counts, first_difference=first)

def calls(path, arm):
    result, current, total, earlier = [], None, 0, 0
    for line in path.open():
        if line.startswith('#'):
            continue
        row = line.rstrip('\n').split('\t')
        assert len(row) == 22 and int(row[1]) == total
        total += 1
        event, data, conflict = row[2], row[-1], int(row[3])
        assert 'truncat' not in event.lower()
        if event == 'trace_begin':
            continue
        if conflict != 19375:
            assert arm == 'kissat' and 0 <= conflict < 19375
            assert event in ('sweep_schedule', 'sweep_seed', 'sweep_environment', 'sweep_effort')
            earlier += 1
            continue
        begin = event == 'sweep_begin' or (event == 'slot_boundary' and data == 'name=Sweep|stage=begin')
        end = event == 'sweep_end' or (event == 'slot_boundary' and data == 'name=Sweep|stage=end')
        if begin:
            assert current is None
            current = {kind: [] for kind in ('sweep_seed', 'sweep_environment', 'sweep_effort', 'sweep_unit')}
        elif end:
            assert current is not None
            result.append(current)
            current = None
        elif current is not None and event in current:
            current[event].append(dict(field.split('=', 1) for field in data.split('|')))
    assert current is None and 0 < total < 2000000
    # A changed earlier trajectory may bypass this selected conflict. Preserve
    # that finding rather than manufacturing matching Sweep calls.
    return result, dict(total_rows=total, earlier_native=earlier)

def main():
    assert first_difference([], []) is None
    assert first_difference([1, 2], [1, 2]) is None
    assert first_difference([1, 2], [1, 3])['index'] == 1
    assert first_difference([1], [1, 2])['candidate'] is None
    assert first_difference([1, 2], [1])['comparison'] is None
    destination = c / 'repair-localization.json'
    assert not destination.exists()
    m = json.loads((c / 'manifest.json').read_text())
    old = root / m['parent_campaign']
    for name, digest in m['hashes'].items():
        assert sha(c / name) == digest
    assert sha(root / m['input']) == m['input_sha256_compressed']
    old_candidate = json.loads((old / 'results/candidate.json').read_text())
    assert sha(old / 'traces/candidate.decisions.tsv') == old_candidate['trace_sha256']['candidate.decisions.tsv']
    evidence, streams, sweeps, observers = {}, {}, {}, {}
    for arm in ('candidate', 'kissat'):
        path = c / 'results' / f'{arm}.json'
        record = json.loads(path.read_text())
        assert record['exit_code'] == 0 and record['scratch_deleted']
        assert record['partial_certificate']['bytes'] > 0
        assert sha(c / 'pins' / arm) == record['binary_sha256']
        for name, digest in record['trace_sha256'].items():
            assert sha(c / 'traces' / name) == digest
        evidence[arm] = dict(result_sha256=sha(path), trace_sha256=record['trace_sha256'])
        sweeps[arm], observers[arm] = calls(c / 'traces' / f'{arm}.sweep.tsv', arm)
        prefix = [line.split('\t') for line in (c / 'traces' / f'{arm}.prefix.tsv').read_text().splitlines() if not line.startswith('#')]
        assert len(prefix) == 1 and int(prefix[0][1]) == m['stop_conflict']
        streams[arm] = int(prefix[0][4])
    old_native = json.loads((old / 'results/kissat.json').read_text())
    for suffix in ('decisions', 'prefix', 'boundaries', 'sweep'):
        name = f'kissat.{suffix}.tsv'
        assert sha(old / 'traces' / name) == old_native['trace_sha256'][name]
        assert old_native['trace_sha256'][name] == evidence['kissat']['trace_sha256'][name]
    comparisons = {}
    for label, path in [('native', c / 'traces/kissat.decisions.tsv'),
                        ('previous', old / 'traces/candidate.decisions.tsv')]:
        comparisons[label] = compare_streams(c / 'traces/candidate.decisions.tsv', path)
        assert comparisons[label]['counts'][0] == streams['candidate']
    assert comparisons['native']['counts'][1] == streams['kissat']
    selected = []
    for index, (a, b) in enumerate(itertools.zip_longest(sweeps['candidate'], sweeps['kissat'])):
        if a is None or b is None:
            selected.append(dict(call=index + 1, missing='candidate' if a is None else 'kissat'))
            continue
        difference = {}
        for kind, keys in [('sweep_seed', ('external',)), ('sweep_environment', ('external', 'vars', 'clauses')),
                           ('sweep_effort', ('external', 'delta', 'solves'))]:
            projected = lambda rows: [{key: row[key] for key in keys} for row in rows]
            difference[kind] = first_difference(projected(a[kind]), projected(b[kind]))
        selected.append(dict(call=index + 1, counts={arm: {kind: len(rows) for kind, rows in data.items()}
            for arm, data in [('candidate', a), ('kissat', b)]}, first_differences=difference,
            seed836_present={arm: any(row['external'] == '836' for row in data['sweep_seed'])
                for arm, data in [('candidate', a), ('kissat', b)]},
            units=dict(candidate=a['sweep_unit'], kissat=b['sweep_unit'])))
    result = dict(created_at=datetime.now(timezone.utc).isoformat(), clean_stops=2,
        observer=evidence, event_validation=observers, decision_comparisons=comparisons,
        calls=selected, limitation='Native whole parent replay plus candidate localization; no timing, checked complete verdict, or claim that entry eligibility closes all scheduling parity.')
    destination.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: result[key] for key in ('decision_comparisons', 'calls')}, indent=2))

if __name__ == '__main__':
    main()
