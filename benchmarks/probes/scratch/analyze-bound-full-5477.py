#!/usr/bin/env python3
"""Rank remaining losses from complete bound/phase-budget solver snapshots."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260907-5454-eliminate-limit-ranks1-40/solver-checkpoint.json'
new = runs / '20260907-5465-bound-advance-ranks1-40/solver-checkpoint.json'
if not new.exists():
    print('5477 analysis pending complete 5465 solver snapshot')
    raise SystemExit(0)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
views = [json.loads(p.read_text()) for p in (old, new)]
for v in views:
    assert v['solver_records'] == 80 and set(v['per_rank']) == set(map(str, range(1, 41)))
    for pair in v['per_rank'].values():
        assert set(pair) == {'tactix', 'kissat'}
        for r in pair.values(): assert sha(root / r['record']) == r['record_sha256']
rows = []
for rank, pair in views[1]['per_rank'].items():
    previous = views[0]['per_rank'][rank]
    t, k = pair['tactix'], pair['kissat']
    oldt = previous['tactix']
    records = {a: json.loads((root / r['record']).read_text()) for a, r in pair.items()}
    assert records['tactix']['instance'] == records['kissat']['instance']
    assert records['tactix']['instance'] == json.loads((root / oldt['record']).read_text())['instance']
    row = dict(rank=int(rank), instance=records['tactix']['instance'],
        candidate_seconds=t['seconds'], kissat_seconds=k['seconds'],
        seconds_over_kissat=t['seconds'] - k['seconds'], time_ratio=t['seconds'] / k['seconds'],
        work=t['work'], kissat_work=k['work'], prior_candidate_work=oldt['work'],
        work_unchanged_from_prior=t['work'] == oldt['work'],
        candidate_peak_rss_kb=t['peak_rss_kb'], kissat_peak_rss_kb=k['peak_rss_kb'],
        work_ratios={n: t['work'][n] / k['work'][n]
            if t['work'][n] is not None and k['work'][n] else None for n in t['work']},
        verification={a: r['verification'] for a, r in pair.items()},
        component_statistics={a: [line for line in r['solver_statistics']
            if re.match(r'^c\s+(?:timing\b|sweep\b|bve\b|factor\b|vivify\b|search\b|simplify\b)', line)]
            for a, r in records.items()})
    rows.append(row)
rows.sort(key=lambda r: -r['seconds_over_kissat'])
out = dict(source_sha256={str(p.relative_to(root)): sha(p) for p in (old, new)},
    full_view_ratio=views[1]['provisional_all_solved_time_ratio'],
    checked_at_solver_snapshot=views[1]['checked_clean'], ranked_losses=rows,
    candidate_work_changed_ranks=sorted(r['rank'] for r in rows if not r['work_unchanged_from_prior']),
    decision='Prioritize the largest remaining measured losses and their pending causal traces. Bounded Rank-37 closure does not establish full-run trajectory equality. Keep source-backed general repairs while investigating remaining work and cost.',
    limitations='Complete same-campaign Tactix/Kissat scoring view; predecessor comparison crosses windows and does not causally attribute timing. Counter ratios and unmatched timer scopes do not prove unit-cost parity or infer weakness from raw work counts. Per-rank peak RSS is not a live-memory ownership decomposition.')
p = new.parent / 'next-losses.json'
assert not p.exists()
p.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps({k: v for k, v in out.items() if k != 'ranked_losses'}, indent=2))
print(json.dumps([{k: v for k, v in r.items() if k != 'component_statistics'} for r in rows[:5]], indent=2))
