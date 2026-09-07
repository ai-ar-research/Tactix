#!/usr/bin/env python3
"""Validate the full fixed Rank 1–40 run before scoring its runtime objective."""
import collections
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5383-vivify-retry-ranks1-40'
files = sorted((c / 'results').glob('*.json'))
if len(files) != 80:
    print('5383 pending', len(files), 'of 80'); raise SystemExit(0)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['pin_sha256'].items(): assert sha(c / rel) == expected
groups = collections.defaultdict(list); pairs = collections.defaultdict(dict)
for p in files:
    r = json.loads(p.read_text())
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
    solved = r['solve']['exit_code'] in (10, 20) and not r['solve']['timed_out']
    if solved:
        assert r['valid'] and r['expected_match']
        check = r['model_check'] if r['solve']['exit_code'] == 10 else r['proof_check']
        assert check['exit_code'] == 0 and not check['timed_out']
    else:
        assert r['solve']['timed_out'], 'Non-timeout failed verdict requires diagnosis, not scoring exclusion'
    groups[r['arm']].append(r)
    pairs[r['rank']][r['arm']] = dict(solved=solved, seconds=r['solve']['wall_seconds'], peak_rss_kb=r['solve']['peak_rss_kb'])
assert len(pairs) == 40 and all(set(p) == {'tactix', 'kissat'} for p in pairs.values())
limit = m['resource_contract']['solve_timeout']
totals = {}
for a, rows in groups.items():
    solved = [r for r in rows if r['valid'] and not r['solve']['timed_out']]
    totals[a] = dict(solved=len(solved), sat=sum(r['solve']['exit_code'] == 10 for r in solved), unsat=sum(r['solve']['exit_code'] == 20 for r in solved),
                     timeouts=40-len(solved), seconds=sum(r['solve']['wall_seconds'] for r in solved),
                     par2=sum(r['solve']['wall_seconds'] if r in solved else 2*limit for r in rows),
                     max_rss_kb=max(r['solve']['peak_rss_kb'] or 0 for r in rows))
result = dict(final_records=80, arms=totals, per_rank={str(k): v for k, v in sorted(pairs.items())},
              all_solved_time_ratio=(totals['tactix']['seconds']/totals['kissat']['seconds'] if all(t['solved'] == 40 for t in totals.values()) else None),
              par2_ratio=totals['tactix']['par2']/totals['kissat']['par2'], result_sha256={p.name: sha(p) for p in files},
              limitations='Full requested scoring view, not annual/union Tier D. Isolated cumulative candidate UNFUZZED and unpromoted; B28+C5 and consolidation fuzz remain separate gates.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: v for k, v in result.items() if k not in ('per_rank', 'result_sha256')}))
