#!/usr/bin/env python3
"""Measure full-set transfer without promoting the isolated repair."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
import fixed_runtime as f
from checked_comparison import classify

c = root / 'benchmarks/runs/20260910-7053-collect-retry-ranks1-50'
assert not c.exists()
gate = root / 'benchmarks/runs/20260910-7034-collect-retry-BC-gate'
timing = root / 'benchmarks/runs/20260910-7041-collect-retry-runtime'
evidence = {}
def bind(p): evidence[str(p.relative_to(root))] = f.sha(p)
for campaign, count in ((gate, 66), (timing, 12)):
    m = json.loads((campaign / 'manifest.json').read_text())
    bind(campaign / 'manifest.json')
    for rel, digest in m['pin_sha256'].items():
        assert f.sha(campaign / rel) == digest, rel
    for task in range(count):
        p = campaign / f'results/{task:03d}.json'
        final = p.exists()
        if not final: p = campaign / f'provisional/{task:03d}.json'
        r = json.loads(p.read_text())
        state = classify(r, final)
        assert state['status'] in ('checked_SAT', 'checked_UNSAT', 'pending_validation'), (p, state)
        assert r['expected_match'] and not r['solve']['timed_out']
        bind(p)
checkpoint = timing / 'solver-checkpoint-7044.json'
s = json.loads(checkpoint.read_text())
assert s['solver_outcomes'] == 12 and s['exact_own_repeats']
assert set(s['ranks']) == {'31', '35'}
assert all(len(r['waves']) == 3 and r['absolute_range_within_two_percent'] for r in s['ranks'].values())
for p in (checkpoint, gate / 'summary-7036.json', gate / 'work-classification-7043.json',
          root / 'benchmarks/runs/20260910-7038-collect-retry-prefix/summary-7040.json',
          root / 'benchmarks/runs/20260910-7050-collect-retry-rank31-late/summary-7052.json', Path(__file__)):
    bind(p)
f.prepare(c, root / 'benchmarks/runs/20260910-7030-collect-retry-build',
          root / 'build/pin-7030-collect-retry-release/tactix',
          root / 'build/pin-7030-collect-retry-bc/tactix')
m = json.loads((c / 'manifest.json').read_text())
m.update(evidence_sha256=evidence,
    parent_comparison='benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50',
    hypothesis='Source-backed satisfied-large collection advice repair improves Rank35 median29.2% but regresses Rank31 median9.8%. Measure all50 unchanged ranks, including adverse effects, against frozen native and6730 results.',
    decision='Provisional measurement only, not source promotion. No native refresh, deadline extension, option tuning or change to existing campaigns. Source fuzz remains unavailable; pending certificates permit development, not acceptance.',
    test_consideration=m['test_consideration'] + ' Solver and runtime harness unchanged. Existing fixed-runtime and checked-comparison fixtures cover preparation/scoring and invalidity classification; complete66 B/C plus12 timing outcomes are explicitly required. No new parser or solver behavior.')
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
