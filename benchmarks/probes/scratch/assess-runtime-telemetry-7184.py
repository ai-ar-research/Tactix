#!/usr/bin/env python3
"""Freeze all three telemetry pairs; no acceptance from a selected wave."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import re
import statistics
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260910-7175-runtime-telemetry-runtime'
target = root / 'benchmarks/reports/2026-09-10-runtime-telemetry-assessment-7184.json'
assert sys.argv[1:] in ([], ['--self-test'])

def aggregate(previous, candidate):
    assert len(previous) == len(candidate) == 3
    assert all(v > 0 for v in previous + candidate)
    ratios = [b/a for a,b in zip(previous, candidate)]
    spreads = [max(v)/min(v)-1 for v in (previous, candidate)]
    return dict(ratios=ratios, median=statistics.median(ratios), range=[min(ratios), max(ratios)],
                previous_seconds=previous, candidate_seconds=candidate,
                within_arm_spreads=spreads, absolute_headline_stable=max(spreads) <= .02)

def self_test():
    r = aggregate([100, 100, 100], [101, 99, 100])
    assert r['ratios'] == [1.01, .99, 1.] and r['median'] == 1.
    assert not r['absolute_headline_stable']
    assert aggregate([100, 100, 100], [100, 100, 100])['absolute_headline_stable']
    for a,b in (([], []), ([1, 1], [1, 1]), ([1, 1, 0], [1, 1, 1])):
        try: aggregate(a,b)
        except AssertionError: pass
        else: raise AssertionError('bad pair accepted')
    print('telemetry assessment: paired median/full-range/spread and three negative controls PASS')
self_test()
if sys.argv[1:]: sys.exit(0)
assert not target.exists()
validator = Path(__file__).with_name('check-submit-runtime-telemetry-7175.py')
subprocess.run([sys.executable, str(validator), 'runtime'], check=True)
spec = importlib.util.spec_from_file_location('telemetry_contract', c / 'pins/telemetry_contract.py')
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p): hashes[str(p.relative_to(root))] = sha(p)
for p in (Path(__file__), validator, c / 'manifest.json', c / 'matrix.tsv'): bind(p)
for wave in range(3): bind(c / f'wave{wave}-submission.json')
with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
records, states, phase = {}, {}, {}
for task in range(18):
    p = c / f'results/{task:03d}.json'; final = p.exists()
    if not final: p = c / f'provisional/{task:03d}.json'
    if not p.exists():
        print('Solver wave incomplete; no assessment written'); sys.exit(0)
    r = json.loads(p.read_text()); state = classify(r, final)
    assert state['status'] in ('pending_validation', 'checked_UNSAT'), (task, state)
    assert r['task'] == task and r['arm'] == matrix[task]['arm'] and r['rank'] == int(matrix[task]['rank'])
    h.fingerprint(r, r['arm'] == 'candidate'); bind(p)
    key = (task // 6, r['rank'], r['arm']); assert key not in records
    records[key], states[task] = r, state
    matched = [re.fullmatch(r'c timing +(\d+) ms \([^)]*\) inprocessing slot, (\d+) ms \([^)]*\) search\+scaffolding outside it', s) for s in r['solver_statistics']]
    matched = [v for v in matched if v]; assert len(matched) == 1
    phase[key] = list(map(int, matched[0].groups()))
rows = {}
for rank in (31, 35, 38):
    series = {arm: [records[(wave, rank, arm)] for wave in range(3)] for arm in ('previous', 'candidate')}
    for wave in range(3): assert h.fingerprint(series['previous'][wave]) == h.fingerprint(series['candidate'][wave])
    row = aggregate(*[[r['solve']['wall_seconds'] for r in series[arm]] for arm in ('previous', 'candidate')])
    row['rss_ratios'] = [series['candidate'][w]['solve']['peak_rss_kb']/series['previous'][w]['solve']['peak_rss_kb'] for w in range(3)]
    row['phase_ms'] = {arm: [phase[(w, rank, arm)] for w in range(3)] for arm in ('previous', 'candidate')}
    row['nodes'] = {arm: [r['node'] for r in series[arm]] for arm in ('previous', 'candidate')}
    rows[str(rank)] = row
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
    exact_work_and_proofs=True, outcomes=18, checked_clean=sum(s['checked'] for s in states.values()),
    rows=rows,
    interpretation='Joint binary-telemetry/per-rule-clock ablation. Inspect all paired medians/ranges, RSS and coarse phase decomposition before changing defaults. Removed instrumentation exists but a net runtime gain is not guaranteed. Code displacement and observed placement are confounds, not established causes. No individual counter pricing, proof-completion claim or promotion.',
    next='Keep the larger Factor occurrence-arena investigation7180 independent of this ablation. Final proof/cleanup, memory and current-source fuzz/consolidation gates remain open.')
with target.open('x') as out: json.dump(result, out, indent=2)
print(target)
for rank, row in rows.items(): print(rank, 'median', row['median'], 'range', row['range'], 'RSS', row['rss_ratios'])
