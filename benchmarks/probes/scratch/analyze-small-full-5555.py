#!/usr/bin/env python3
"""Rank all current losses, preserving work, memory and verification scope."""
from pathlib import Path
import hashlib
import json
root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-5485-sweep-cursor-ranks1-40/solver-checkpoint.json'
new = runs / '20260908-5535-sweep-small-ranks1-40/solver-checkpoint.json'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
if not new.exists():
    print('Complete5535 solver snapshot pending')
    raise SystemExit(0)
views = [json.loads(p.read_text()) for p in (old, new)]
for view in views:
    assert view['solver_records'] == 80 and set(view['per_rank']) == set(map(str, range(1, 41)))
    for pair in view['per_rank'].values():
        for row in pair.values(): assert sha(root / row['record']) == row['record_sha256']
rows = []
unchanged = []
for rank, pair in views[1]['per_rank'].items():
    previous = views[0]['per_rank'][rank]
    t, k = pair['tactix'], pair['kissat']
    tr, kr = [json.loads((root / r['record']).read_text()) for r in (t, k)]
    pt, pk = [json.loads((root / previous[a]['record']).read_text()) for a in ('tactix', 'kissat')]
    assert k['work'] == previous['kissat']['work']
    assert kr['certificate_sha256'] == pk['certificate_sha256']
    stable = lambda r: [s for s in r['solver_statistics'] if not s.startswith('c timing')]
    exact = stable(tr) == stable(pt) and tr['certificate_sha256'] == pt['certificate_sha256']
    if exact: unchanged.append(int(rank))
    rows.append(dict(rank=int(rank), instance=tr['instance'],
        candidate_seconds=t['seconds'], kissat_seconds=k['seconds'],
        seconds_over_kissat=t['seconds'] - k['seconds'], time_ratio=t['seconds']/k['seconds'],
        work=t['work'], kissat_work=k['work'],
        work_ratios={name:t['work'][name]/value if value else None for name,value in k['work'].items()},
        prior_candidate_work=previous['tactix']['work'], exact_work_and_proof_unchanged_from_prior=exact,
        candidate_peak_rss_kb=t['peak_rss_kb'], kissat_peak_rss_kb=k['peak_rss_kb'],
        peak_rss_ratio=t['peak_rss_kb']/k['peak_rss_kb'],
        verification={arm:r['verification'] for arm,r in pair.items()}))
rows.sort(key=lambda r:-r['seconds_over_kissat'])
result = dict(source_sha256={str(p.relative_to(root)):sha(p) for p in (old,new)},
    full_view_ratio=views[1]['provisional_all_solved_time_ratio'],
    checked_at_solver_snapshot=views[1]['checked_clean'],
    candidate_exact_work_and_proof_unchanged_ranks=sorted(unchanged),
    all40_native_work_and_proof_controls_unchanged=True, ranked_losses=rows,
    decision='Use the complete ranked losses with ongoing Rank32 causal replay and Rank39 clean profiles. Retain only source-backed general fixes; candidate5546 separately measures unused Vivify census cost with exact-work pairs.',
    limitations='Different full-campaign windows do not establish paired gains from an individual repair. Ratios of work or peak RSS are not scope-matched per-operation costs or live-memory ownership. Pending proofs do not pass a correctness gate; goal remains unproved.')
p = new.parent / 'next-losses.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k:v for k,v in result.items() if k != 'ranked_losses'}, indent=2))
print(json.dumps([{k:r[k] for k in ('rank','time_ratio','seconds_over_kissat','work_ratios','peak_rss_ratio')} for r in rows[:5]], indent=2))
