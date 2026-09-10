#!/usr/bin/env python3
"""Prepare one candidate-only full50 measurement; immutable reference unchanged."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
import fixed_runtime as f
from checked_comparison import classify

c = root / 'benchmarks/runs/20260910-6980-xor-presence-ranks1-50'
assert not c.exists()
gate = root / 'benchmarks/runs/20260910-6957-xor-presence-BC-gate/summary-6968.json'
g = json.loads(gate.read_text())
assert g['solver_records'] == 66 and len(g['pairs']) == 33
assert all(p['reported_work_equal'] and p['certificate_equal'] for p in g['pairs'])
published = sorted((gate.parent / 'results').glob('*.json'))
assert len(published) == 66
for path in published:
    state = classify(json.loads(path.read_text()), True)
    assert state['checked'] and state['clean'], (path, state)
evidence = [
    gate,
    root / 'benchmarks/runs/20260910-6960-xor-presence-runtime/solver-checkpoint-6962.json',
    root / 'benchmarks/runs/20260910-6960-xor-presence-runtime/timer-cost-6963.json',
    root / 'benchmarks/reports/2026-09-10-factor-xor-final-audit-6978.json',
]
bindings = {str(p.relative_to(root)): f.sha(p) for p in evidence}
f.prepare(c, root / 'benchmarks/runs/20260910-6950-xor-presence-build',
          root / 'build/pin-6950-xor-presence-release/tactix',
          root / 'build/pin-6950-xor-presence-bc/tactix')
m = json.loads((c / 'manifest.json').read_text())
m.update(
    provisional_gate=dict(path=str(gate.relative_to(root)), sha256=f.sha(gate),
                          expected=66, checked=66),
    evidence_sha256=bindings,
    parent_comparison='benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50',
    hypothesis='Reuse already indexed ternary XOR sign membership without changing extraction order, ticks, policy or proof. Targeted Rank47/42 pairs improved; Rank38 neutral. Measure all50 including adverse results.',
    decision='Measurement only. Current source authority remains6730; production and fixed7239.010s Kissat reference unchanged. Do not infer full50 benefit from targeted pairs. Independent checks run asynchronously; unavailable source fuzz still blocks promotion.',
    test_consideration=m['test_consideration'] + ' No solver/parser/scoring change here. Reuse maintained fixed-runtime and checked-comparison tests; assert all66 B/C expected checked results and complete33 exact-work/certificate pairs before preparation.')
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
