#!/usr/bin/env python3
"""Select the next current-source loss from all40 pairs, never a partial aggregate."""
import json
from pathlib import Path
import runpy
s = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root, sha = s['root'], s['sha']
c = root / 'benchmarks/runs/20260908-5604-factor-support-ranks1-40'
p = c / 'solver-checkpoint.json'
if not p.exists():
    print('Complete5604 checkpoint pending'); raise SystemExit(0)
current = json.loads(p.read_text())
old_path = root / 'benchmarks/runs/20260908-5535-sweep-small-ranks1-40/solver-checkpoint.json'
old = json.loads(old_path.read_text())
assert current['solver_records'] == old['solver_records'] == 80
assert set(current['per_rank']) == set(old['per_rank']) == {str(i) for i in range(1,41)}
losses = []
for rank, pair in current['per_rank'].items():
    previous = old['per_rank'][rank]['tactix']
    records = {}
    for name, arm in [('tactix', pair['tactix']), ('kissat', pair['kissat']), ('previous', previous)]:
        record_path = root / arm['record']
        assert sha(record_path) == arm['record_sha256']
        records[name] = json.loads(record_path.read_text())
    t, k = pair['tactix'], pair['kissat']
    a, b = records['tactix'], records['previous']
    assert a['instance'] == b['instance'] == records['kissat']['instance']
    work = {key: dict(tactix=t['work'].get(key), kissat=k['work'].get(key))
            for key in sorted(t['work'].keys() | k['work'].keys())}
    changes = [[x,y] for x,y in __import__('itertools').zip_longest(
        [x for x in a['solver_statistics'] if not x.startswith('c timing')],
        [x for x in b['solver_statistics'] if not x.startswith('c timing')]) if x != y]
    losses.append(dict(rank=int(rank), tactix_seconds=t['seconds'], kissat_seconds=k['seconds'],
        extra_seconds=t['seconds']-k['seconds'], time_ratio=t['seconds']/k['seconds'],
        work=work, same_parent_certificate=a['certificate_sha256'] == b['certificate_sha256'],
        parent_statistics_differences=changes,
        memory_ratio=t['peak_rss_kb']/k['peak_rss_kb']))
losses.sort(key=lambda r:(-r['extra_seconds'],r['rank']))
result = dict(checkpoint_sha256=sha(p), parent_checkpoint_sha256=sha(old_path),
    all40_ratio=current['provisional_all_solved_time_ratio'], losses=losses,
    interpretation='Current full-window losses choose the next diagnostic. Work counters have different embedded/search scopes across solvers; throughput ratios do not isolate unit cost. Parent timing was another window, so no cross-window causal speedup claim. Pending proofs/fuzz block promotion, not diagnosis.')
target = c / 'next-losses.json'
if target.exists(): assert json.loads(target.read_text()) == result
else: target.write_text(json.dumps(result, indent=2) + '\n')
print('All40 ratio:', result['all40_ratio'])
print(json.dumps([{k:r[k] for k in ('rank','tactix_seconds','kissat_seconds','extra_seconds','time_ratio','same_parent_certificate')} for r in losses[:8]], indent=2))
