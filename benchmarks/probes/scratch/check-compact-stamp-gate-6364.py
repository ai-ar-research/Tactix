#!/usr/bin/env python3
"""Freeze a complete solver checkpoint without waiting for final certificates."""
import csv
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root/'benchmarks/runs/20260909-6362-compact-stamp-BC-gate'
target = c/'solver-checkpoint-6364.json'
assert not target.exists()
m = json.loads((c/'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(c/rel) == digest
with (c/'matrix.tsv').open() as stream:
    matrix = {int(r['task']):r for r in csv.DictReader(stream, delimiter='\t')}
latest = {}
for folder in ('provisional', 'results'):
    for p in (c/folder).glob('*.json'):
        r = json.loads(p.read_text())
        state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
        for key in ('arm', 'instance', 'expected'):
            assert r[key] == matrix[r['task']][key]
        # The frozen runner publishes binary provenance in final records only.
        if folder == 'results':
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        latest[r['task']] = (p, r, state)
if len(latest) != 66:
    print('Solver gate pending:', len(latest), '/66; no checkpoint created')
    sys.exit(0)
assert set(latest) == set(range(66))
by_tier = {}
for tier in ('B', 'C'):
    sums = {}
    for _, r, _ in latest.values():
        if r['arm'].endswith('_'+tier):
            sums[r['arm']] = sums.get(r['arm'], 0) + r['solve']['wall_seconds']
    assert len(sums) == 2
    base = next(v for k, v in sums.items() if not k.startswith('candidate'))
    by_tier[tier] = dict(sums=sums, candidate_over_production=sums['candidate_'+tier]/base)
result = dict(solver_records=66, checked=sum(s['checked'] for _, _, s in latest.values()),
    by_tier=by_tier, hashes={str(p.relative_to(root)):sha(p) for p, _, _ in latest.values()},
    manifest_sha256=sha(c/'manifest.json'),
    limitation='Expected solver verdicts allow provisional development; pending checks remain unverified and block promotion. No Tier-D performance claim.')
target.write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
