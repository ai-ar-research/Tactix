#!/usr/bin/env python3
"""Freeze provisional solver views without treating pending certificates as checked."""
import datetime
import hashlib
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from runtime_summary import counters
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
c = root / 'benchmarks/runs/20260907-5407-provisional-runtime-views'
c.mkdir(exist_ok=False)
views = {}
for number in (5383, 5395):
    campaign = next((root / 'benchmarks/runs').glob(f'20260907-{number}-*'))
    rows = {}
    for kind in ('provisional', 'results'):
        for p in sorted((campaign / kind).glob('*.json')):
            row = json.loads(p.read_text())
            checked = kind == 'results'
            if checked: assert row['valid'] and row['artifact_deleted'], p
            assert row['expected_match'] and not row['solve']['timed_out']
            assert row['solve']['exit_code'] == (10 if row['verdict'] == 'SAT' else 20)
            key = (row['rank'], row['arm'])
            rows[key] = dict(rank=row['rank'], arm=row['arm'], instance=row['instance'],
                             verdict=row['verdict'], checked_clean=checked,
                             wall_seconds=row['solve']['wall_seconds'], rss_kib=row['solve']['peak_rss_kb'],
                             work=counters(row), source=str(p.relative_to(root)), source_sha256=sha(p))
    assert len(rows) == 80
    totals = {a: sum(r['wall_seconds'] for r in rows.values() if r['arm'] == a) for a in ('tactix', 'kissat')}
    views[str(number)] = dict(campaign=str(campaign.relative_to(root)), manifest_sha256=sha(campaign / 'manifest.json'),
                              totals=totals, provisional_ratio=totals['tactix']/totals['kissat'],
                              checked_clean=sum(r['checked_clean'] for r in rows.values()),
                              rows=list(rows.values()))
comparison = []
before = {(r['rank'], r['arm']): r for r in views['5383']['rows']}
after = {(r['rank'], r['arm']): r for r in views['5395']['rows']}
for rank in range(1, 41):
    a, b = before[rank, 'tactix'], after[rank, 'tactix']
    comparison.append(dict(rank=rank, before_seconds=a['wall_seconds'], after_seconds=b['wall_seconds'],
                           seconds_delta=b['wall_seconds']-a['wall_seconds'],
                           before_work=a['work'], after_work=b['work'],
                           checked_both=a['checked_clean'] and b['checked_clean']))
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              purpose='User-approved asynchronous proof-checking workflow: use expected solver results provisionally to continue dependent work, revisit actual checker outcomes.',
              validation='All 80 expected solver exits per view present, no known rejected final result. Pending certificates are NOT counted as verified.',
              views=views, candidate_cross_window_comparison=comparison,
              limitations='Different full-view windows: cross-window deltas locate mechanisms and regressions, not replicated paired effect sizes. No final promotion or checked-gate closure inferred.')
(c / 'manifest.json').write_text(json.dumps(dict(purpose=result['purpose'], source_campaigns={k:v['campaign'] for k,v in views.items()}, no_solver_execution=True), indent=2) + '\n')
(c / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k:{f:v[f] for f in ('totals','provisional_ratio','checked_clean')} for k,v in views.items()}, indent=2))
print(json.dumps(sorted(comparison,key=lambda r:abs(r['seconds_delta']),reverse=True)[:5], indent=2))
