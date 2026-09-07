#!/usr/bin/env python3
"""Require all 15 checked models before comparing the crossed ablation."""
from collections import defaultdict
import hashlib
import importlib.util
import json
from pathlib import Path

C = Path(__file__).resolve().parent
ROOT = C.parents[2]
spec = importlib.util.spec_from_file_location('runtime_summary',ROOT/'tools/bench/runtime_summary.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
paths = [C/'results'/f'{i:03d}.json' for i in range(15)]
rows = [json.loads(p.read_text()) for p in paths]
by_rank = defaultdict(dict)
for r in rows:
    assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    assert r['expected'] == 'SAT' and r['expected_match'] and r['model_check']['result'] == 'verified'
    assert r['model_check']['exit_code'] == 0 and r['solve']['exit_code'] == 10 and not r['solve']['timed_out']
    by_rank[r['rank']][r['arm']] = dict(seconds=r['solve']['wall_seconds'],rss_kib=r['solve']['peak_rss_kb'],
        counters=module.counters(r),certificate_sha256=r['certificate_sha256'])
for rank,arms in by_rank.items():
    assert set(arms) == {'current','round_only','bridge','bridge_round','kissat'}
    for arm,r in arms.items():
        r['ratio_current'] = r['seconds']/arms['current']['seconds']
        r['ratio_kissat'] = r['seconds']/arms['kissat']['seconds']
totals = {arm:sum(by_rank[rank][arm]['seconds'] for rank in by_rank) for arm in next(iter(by_rank.values()))}
print(json.dumps(dict(checked_clean=15,invalid=0,verdicts=dict(SAT=15,UNSAT=0),timeout_transitions=0,
    by_rank=by_rank,total_seconds=totals,par2_mean_seconds={arm:t/3 for arm,t in totals.items()},
    scope='Single diagnostic wave on three predeclared witnesses. No repeat interval, global performance conclusion, or candidate promotion.',
    result_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}),indent=2))
