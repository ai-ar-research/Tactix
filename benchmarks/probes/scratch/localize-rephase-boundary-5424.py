#!/usr/bin/env python3
"""Align portable rule boundaries after validated full-stream comparison."""
import hashlib
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5424-rank35-rephase-prefix'
summary = json.loads((c / 'summary.json').read_text())
assert summary['clean_stops'] == 2 and summary['prior_13833_prefix_preserved']
names = {'congruence': 'Congruence', 'substitute_early': 'Substitute',
         'substitute_late': 'Substitute', 'backbone_early': 'Backbone',
         'backbone_late': 'Backbone', 'vivify': 'Vivify', 'sweep': 'Sweep',
         'transitive': 'TransitiveReduce', 'factor': 'Factor',
         'strengthen': 'Strengthen', 'bve': 'BVE'}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
boundaries = {}
for arm in ('candidate', 'kissat'):
    p = c / 'traces' / (arm + '.boundaries.tsv')
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert sha(p) == r['trace_sha256'][p.name]
    rows = []
    for line in p.read_text().splitlines():
        if line.startswith('#'): continue
        row = line.split('\t')
        if int(row[4]) == 0 or row[2] not in (names if arm == 'kissat' else names.values()): continue
        rows.append(dict(name=names.get(row[2], row[2]), stage=row[3], conflicts=int(row[4]),
            decisions=int(row[5]), active=int(row[7]), irred=int(row[10]),
            red=int(row[11]), binary=int(row[12])))
    boundaries[arm] = rows
first = None
for index, (t, k) in enumerate(itertools.zip_longest(boundaries['candidate'], boundaries['kissat'])):
    if t != k:
        first = dict(index=index, candidate=t, kissat=k); break
assert first is not None
result = dict(first_coarse_boundary=first,
    preceding_boundaries={a: rows[max(0, first['index']-4):first['index']+1] for a, rows in boundaries.items()},
    decision_difference=summary['first_decision_difference'],
    limitation='Active and clause counts locate a boundary, not formula/stream equality or relative semantic strength. More active-variable removal alone does not justify acceptance or a fidelity repair.')
p = c / 'boundary-localization.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
