#!/usr/bin/env python3
"""Final checked proof-on Rank 1–40 gate; no provisional records allowed."""
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import statistics

C = Path(__file__).resolve().parent
ROOT = C.parents[2]
spec = importlib.util.spec_from_file_location('runtime_summary', ROOT/'tools/bench/runtime_summary.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
paths = sorted((C/'results').glob('*.json'))
assert len(paths) == 80
rows = [json.loads(p.read_text()) for p in paths]
assert {(r['rank'], r['arm']) for r in rows} == {(rank, arm) for rank in range(1,41) for arm in ('tactix','kissat')}
for r in rows:
    assert r['valid'] and r['expected_match'] and r['artifact_deleted'] and r['derived_artifacts_deleted']
    assert not r['cleanup_pending'] and not r['solve']['timed_out']
    assert r['solve']['exit_code'] == (10 if r['expected'] == 'SAT' else 20)
    check = r['model_check'] if r['expected'] == 'SAT' else r['proof_check']
    assert check['exit_code'] == 0
arms = {}
for arm in ('tactix','kissat'):
    rs = [r for r in rows if r['arm'] == arm]
    seconds = sum(r['solve']['wall_seconds'] for r in rs)
    cs = [module.counters(r) for r in rs]
    assert all(v is not None for c in cs for v in c.values())
    arms[arm] = dict(solved=len(rs), validated=len(rs), verdicts=dict(Counter(r['expected'] for r in rs)),
                     solve_seconds=seconds, par2_mean_seconds=seconds/40,
                     peak_rss_kib=max(r['solve']['peak_rss_kb'] for r in rs),
                     counters={key:sum(c[key] for c in cs) for key in cs[0]})
by_key = {(r['rank'],r['arm']):r for r in rows}
ratios = [by_key[i,'tactix']['solve']['wall_seconds']/by_key[i,'kissat']['solve']['wall_seconds'] for i in range(1,41)]
print(json.dumps(dict(checked_clean=80, pending=0, invalid=0, arms=arms,
    aggregate_ratio=arms['tactix']['solve_seconds']/arms['kissat']['solve_seconds'],
    paired_ratio_median=statistics.median(ratios), paired_ratio_min=min(ratios), paired_ratio_max=max(ratios),
    timeout_transitions=0, goal_met=False,
    uncertainty='One matched full-width wave; no repeat confidence interval. Counter equality is not causal attribution.',
    result_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}),indent=2))
