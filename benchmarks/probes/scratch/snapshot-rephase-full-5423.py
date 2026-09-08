#!/usr/bin/env python3
"""Score all forty solver pairs provisionally, preserving checker status."""
import collections
import datetime
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from runtime_summary import counters
c = root / 'benchmarks/runs/20260907-5423-rephase-root-ranks1-40'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
records, paths = {}, {}
for kind in ('provisional', 'results'):
    for p in sorted((c / kind).glob('*.json')):
        r = json.loads(p.read_text()); records[r['task']], paths[r['task']] = r, p
if len(records) != 80:
    print('5423 solver records pending', len(records), 'of 80'); raise SystemExit(0)
assert set(records) == set(range(80))
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest
groups, pairs = collections.defaultdict(list), collections.defaultdict(dict)
for task, r in sorted(records.items()):
    assert r['expected_match'] and not r['solve']['timed_out']
    assert r['solve']['exit_code'] == (10 if r['expected'] == 'SAT' else 20)
    if paths[task].parent.name == 'results':
        assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted']
        assert not r['cleanup_pending']
        assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        check = r['model_check'] if r['expected'] == 'SAT' else r['proof_check']
        assert check['exit_code'] == 0 and not check['timed_out']
    groups[r['arm']].append(r)
    pairs[r['rank']][r['arm']] = dict(seconds=r['solve']['wall_seconds'],
        peak_rss_kb=r['solve']['peak_rss_kb'], work=counters(r),
        verification='checked' if r.get('valid') else 'pending',
        record=str(paths[task].relative_to(root)), record_sha256=sha(paths[task]))
assert set(pairs) == set(range(1, 41))
assert all(set(p) == {'tactix', 'kissat'} for p in pairs.values())
totals = {a: dict(expected_solves=40, sat=sum(r['expected'] == 'SAT' for r in rows),
    unsat=sum(r['expected'] == 'UNSAT' for r in rows), timeouts=0,
    solver_seconds=sum(r['solve']['wall_seconds'] for r in rows),
    provisional_par2=sum(r['solve']['wall_seconds'] for r in rows),
    max_rss_kb=max(r['solve']['peak_rss_kb'] or 0 for r in rows)) for a, rows in groups.items()}
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    solver_records=80, checked_clean=sum(r.get('valid', False) for r in records.values()),
    manifest_sha256=sha(c / 'manifest.json'), arms=totals,
    provisional_all_solved_time_ratio=totals['tactix']['solver_seconds']/totals['kissat']['solver_seconds'],
    per_rank={str(rank): p for rank, p in sorted(pairs.items())},
    limitation='All forty pairs, not a partial aggregate. Pending certificates are provisional passes for investigation, not final checked scoring or promotion. Isolated candidate remains UNFUZZED; annual/union Tier D and memory gates remain separate.')
p = c / 'solver-checkpoint.json'
assert not p.exists(), 'Frozen checkpoint already exists'
p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: v for k, v in result.items() if k != 'per_rank'}, indent=2))
