#!/usr/bin/env python3
"""Split selected Sweep events by invocation before comparing seed/effort order."""
import hashlib
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5490-rank32-sweep'
validation = c / 'observer-validation.json'
assert json.loads(validation.read_text())['clean_stops'] == 2
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
arms = {}
for arm in ('candidate', 'kissat'):
    path = c / 'traces' / (arm + '.sweep.tsv')
    record = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert sha(path) == record['trace_sha256'][path.name]
    calls, current = [], None
    for line in path.open():
        if line.startswith('#'):
            continue
        row = line.rstrip('\n').split('\t')
        if row[3] != '19375':
            continue
        event, data = row[2], row[-1]
        begin = (event == 'slot_boundary' and data == 'name=Sweep|stage=begin') or event == 'sweep_begin'
        end = (event == 'slot_boundary' and data == 'name=Sweep|stage=end') or event == 'sweep_end'
        if begin:
            assert current is None
            current = {k: [] for k in ('sweep_seed', 'sweep_environment', 'sweep_effort', 'sweep_unit')}
        elif end:
            assert current is not None
            calls.append(current)
            current = None
        elif current is not None and event in current:
            current[event].append(dict(x.split('=', 1) for x in data.split('|')))
    assert current is None and len(calls) == 2
    arms[arm] = calls
comparisons = []
for i, (a, b) in enumerate(zip(arms['candidate'], arms['kissat'])):
    differences = {}
    for kind, keys in (('sweep_seed', ('external',)),
                       ('sweep_environment', ('external', 'vars', 'clauses')),
                       ('sweep_effort', ('external', 'delta', 'solves'))):
        first = next((dict(index=j, candidate=x, kissat=y)
                      for j, (x, y) in enumerate(itertools.zip_longest(a[kind], b[kind]))
                      if x is None or y is None or [x.get(k) for k in keys] != [y.get(k) for k in keys]), None)
        differences[kind] = first
    comparisons.append(dict(call=i + 1, counts={arm: {k: len(v) for k, v in data.items()}
        for arm, data in (('candidate', a), ('kissat', b))},
        first_differences=differences, units=dict(candidate=a['sweep_unit'], kissat=b['sweep_unit'])))
result = dict(observer_validation_sha256=sha(validation), calls=comparisons,
    decision='First call shares14 seed/environment-size/tick-delta/solve-count records, then Tactix processes836 while native advances to1236. Inspect836 eligibility/current occurrences at entry before changing retained scheduling. Native-only unit-103918 occurs later, not yet causally assigned to the skip.',
    limitation='Environment sizes do not prove clause/model equality. Tick origins differ; only deltas are compared. Second-call divergence follows earlier differences. No performance or correctness acceptance.')
p = c / 'sweep-localization.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
