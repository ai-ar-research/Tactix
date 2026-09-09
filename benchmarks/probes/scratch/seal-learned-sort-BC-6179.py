#!/usr/bin/env python3
"""Audit every B28+C5 outcome and predecessor replay without waiting for checks."""
import datetime as dt
import json
import math
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6171-learned-sort-BC-gate'
old = root / 'benchmarks/runs/20260908-6114-substitute-publication-BC-gate'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(c / rel) == digest, rel
rows = {}; statuses = {}; hashes = {}
for folder in ('provisional', 'results'):
    for p in (c / folder).glob('*.json'):
        r = json.loads(p.read_text())
        state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
        parent_path = old / 'results' / p.name
        parent = json.loads(parent_path.read_text())
        if folder == 'results':
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        # Provisional v1 has no command/digest; final identity is mandatory.
        for key in ('instance', 'arm', 'expected', 'certificate_sha256', 'certificate_bytes', 'verdict'):
            assert r[key] == parent[key], (p, key)
        nt = lambda v: [s for s in v['solver_statistics'] if not s.startswith('c timing')]
        assert nt(r) == nt(parent), p
        rows[r['task']], statuses[r['task']] = r, state
        hashes[str(p.relative_to(root))] = sha(p)
        hashes[str(parent_path.relative_to(root))] = sha(parent_path)
assert set(rows) == set(range(66))
totals = {arm: math.fsum(r['solve']['wall_seconds'] for r in rows.values() if r['arm'] == arm) for arm in m['arms']}
checked = sum(v['checked'] for v in statuses.values())
target = c / ('summary.json' if checked == 66 else 'solver-checkpoint.json')
assert not target.exists()
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), solver_results=66,
    checked_clean=checked, exact_predecessor_work_and_proof=66, totals=totals,
    candidate_over_production={t: totals['candidate_'+t]/totals['current_'+t] for t in ('B', 'C')},
    pending_tasks=[t for t, s in statuses.items() if not s['checked']],
    hashes=hashes, manifest_sha256=sha(c / 'manifest.json'),
    limitation='Expected outcomes permit dependent development while certificates run. Pending checks block checked gate closure, not full50 investigation. No performance attribution from cross-window predecessor times.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ('checked_clean', 'candidate_over_production', 'pending_tasks')}, indent=2))
