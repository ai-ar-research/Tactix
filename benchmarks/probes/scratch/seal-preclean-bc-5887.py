#!/usr/bin/env python3
"""Freeze the first complete preclean BC solver view, keeping checks provisional."""
from collections import defaultdict
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5878-forward-preclean-BC-gate'
target = c / 'solver-checkpoint.json'
assert not target.exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest
groups = defaultdict(list)
hashes = {}
for p in (c / 'provisional').glob('*.json'):
    r = json.loads(p.read_text())
    assert r['expected_match'] and not r['solve']['timed_out'] and r['solve']['exit_code'] in (10, 20)
    groups[r['arm']].append(r)
    hashes[str(p.relative_to(root))] = sha(p)
assert {k: len(v) for k, v in groups.items()} == {'candidate_B': 28, 'current_B': 28, 'candidate_C': 5, 'current_C': 5}
totals = {k: sum(r['solve']['wall_seconds'] for r in rs) for k, rs in groups.items()}
finals = list((c / 'results').glob('*.json'))
for p in finals:
    r = json.loads(p.read_text())
    assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    hashes[str(p.relative_to(root))] = sha(p)
out = dict(expected_solves=66, checked_clean=len(finals), totals=totals,
    ratios={tier: totals['candidate_' + tier] / totals['current_' + tier] for tier in ('B', 'C')},
    combined=sum(totals['candidate_' + tier] for tier in ('B', 'C')) / sum(totals['current_' + tier] for tier in ('B', 'C')),
    hashes=hashes, manifest_sha256=sha(c / 'manifest.json'),
    limitation='First complete BC solver-time screen against production,not fullC/D or promotion. Pending certificates unverified;current-source fuzz blocked.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps({k: v for k, v in out.items() if k != 'hashes'}, indent=2))
