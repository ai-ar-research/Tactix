#!/usr/bin/env python3
"""Join per-environment effort to seeds; missing zero-effort rows are not shifts."""
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

def append_event(current, event, data):
    if event == 'sweep_seed':
        current.append(dict(seed=data['external'], environment=None, effort=None))
    elif event in ('sweep_environment', 'sweep_effort'):
        assert current and current[-1]['seed'] == data['external']
        key = 'environment' if event == 'sweep_environment' else 'effort'
        assert current[-1][key] is None
        current[-1][key] = data

def read_calls(path):
    calls, current = [], None
    for line in path.open():
        if line.startswith('#'):
            continue
        row = line.rstrip('\n').split('\t')
        if row[3] != '19375':
            continue
        event, payload = row[2], row[-1]
        if event == 'sweep_begin' or (event == 'slot_boundary' and payload == 'name=Sweep|stage=begin'):
            assert current is None
            current = []
        elif event == 'sweep_end' or (event == 'slot_boundary' and payload == 'name=Sweep|stage=end'):
            assert current is not None
            calls.append(current)
            current = None
        elif current is not None and event in ('sweep_seed', 'sweep_environment', 'sweep_effort'):
            append_event(current, event, dict(item.split('=', 1) for item in payload.split('|')))
    assert current is None and len(calls) == 2
    for call in calls:
        assert all(row['environment'] is not None for row in call)
    return calls

def fixtures():
    rows = []
    append_event(rows, 'sweep_seed', dict(external='1'))
    append_event(rows, 'sweep_environment', dict(external='1', vars='2', clauses='1'))
    append_event(rows, 'sweep_seed', dict(external='2'))
    append_event(rows, 'sweep_environment', dict(external='2', vars='3', clauses='2'))
    append_event(rows, 'sweep_effort', dict(external='2', delta='9', solves='1'))
    assert rows[0]['effort'] is None and rows[1]['effort']['delta'] == '9'
    for event, data in [('sweep_effort', dict(external='1')), ('sweep_environment', dict(external='2')),
                        ('sweep_effort', dict(external='2'))]:
        try:
            append_event(rows, event, data)
        except AssertionError:
            pass
        else:
            raise AssertionError('Misjoined/duplicate event accepted')
    print('PASS: zero-effort join and three negative fixtures', flush=True)

def main():
    fixtures()
    output = c / 'effort-localization.json'
    assert not output.exists()
    validation = c / 'repair-localization.json'
    assert json.loads(validation.read_text())['clean_stops'] == 2
    calls = {}
    for arm in ('candidate', 'kissat'):
        record = json.loads((c / 'results' / f'{arm}.json').read_text())
        path = c / 'traces' / f'{arm}.sweep.tsv'
        assert sha(path) == record['trace_sha256'][path.name]
        calls[arm] = read_calls(path)
    a, b = calls['candidate'][0], calls['kissat'][0]
    assert len(a) == 11030 and len(b) == 11023
    differences, omitted = [], []
    for index, (x, y) in enumerate(zip(a, b)):
        assert x['seed'] == y['seed']
        assert all(x['environment'][key] == y['environment'][key] for key in ('vars', 'clauses'))
        xe, ye = x['effort'], y['effort']
        assert ye is not None
        xcost = (int(xe['delta']), int(xe['solves'])) if xe else (0, 0)
        ycost = (int(ye['delta']), int(ye['solves']))
        if xcost != ycost:
            differences.append(dict(index=index, seed=x['seed'], environment=x['environment'],
                                    candidate_cost=xcost, native_cost=ycost))
        if xe is None:
            omitted.append(dict(index=index, seed=x['seed'], vars=int(x['environment']['vars']),
                                clauses=int(x['environment']['clauses']), native_ticks=ycost[0], native_solves=ycost[1]))
    result = dict(created_at=datetime.now(timezone.utc).isoformat(),
        repair_validation_sha256=sha(validation), common_seed_environments=len(b),
        effort_difference_count=len(differences), first_effort_difference=differences[0] if differences else None,
        omitted_candidate_environments=omitted,
        omitted_native_ticks=sum(row['native_ticks'] for row in omitted),
        common_effort={arm: sum(int(row['effort']['delta']) if row['effort'] else 0 for row in call[:len(b)])
                       for arm, call in [('candidate', a), ('kissat', b)]},
        extra_candidate_seeds=[row['seed'] for row in a[len(b):]],
        extra_candidate_ticks=sum(int(row['effort']['delta']) if row['effort'] else 0 for row in a[len(b):]),
        conclusion='The first differing effort belongs to the same seed/environment, not a shifted effort-array index. Source inspection identifies the vars<2 or clauses<2 early return before initial solve. Test its effects and native accounting before a successor repair.',
        limitation='Equal environment sizes are not equal clauses/models. No claim that every effort or budget discrepancy has one cause, or that repairing this guard improves runtime.')
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items() if key != 'omitted_candidate_environments'}, indent=2))

if __name__ == '__main__':
    main()
