#!/usr/bin/env python3
"""Freeze all fifty expected outcomes and exact parent replay, checks separate."""
import datetime as dt
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
import fixed_runtime as f
from checked_comparison import classify
c = root / 'benchmarks/runs/20260909-6181-learned-sort-ranks1-50'
target = c / 'solver-checkpoint.json'
assert not target.exists()
score = f.score(c)
assert score['records'] == score['expected_solves'] == score['reference_checked'] == 50
rows = {}; hashes = {}
for folder in ('provisional', 'results'):
    for path in (c / folder).glob('*.json'):
        r = json.loads(path.read_text())
        status = classify(r, folder == 'results')
        assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (path, status)
        old = root / 'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50'
        parent_path = old / 'results' / path.name
        if not parent_path.exists(): parent_path = old / 'provisional' / path.name
        parent = json.loads(parent_path.read_text())
        for key in ('certificate_sha256', 'certificate_bytes', 'instance', 'verdict'):
            assert r[key] == parent[key], (path, key)
        nt = lambda x: [s for s in x['solver_statistics'] if not s.startswith('c timing')]
        assert nt(r) == nt(parent), path
        rows[r['rank']] = dict(status, seconds=r['solve']['wall_seconds'], peak_rss_kib=r['solve']['peak_rss_kb'])
        hashes[str(path.relative_to(root))] = f.sha(path)
        hashes[str(parent_path.relative_to(root))] = f.sha(parent_path)
assert len(rows) == 50
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), score=score,
    independent_checked=sum(r['checked'] for r in rows.values()), all50_exact_parent_work_and_proof=True,
    records=rows, hashes=hashes,
    limitation='Complete solver-time view, not final acceptance while any certificate, memory or current-source fuzz gate is pending. Difference from prior full-window timing is not isolated causal attribution.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: v for k, v in score.items() if k != 'per_rank'}, indent=2))
