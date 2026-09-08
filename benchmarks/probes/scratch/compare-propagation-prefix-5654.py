#!/usr/bin/env python3
"""Require complete per-arm trace and partial-proof identity through1M."""
import json
from pathlib import Path
import runpy
support = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root = support['root']
c = root / 'benchmarks/runs/20260908-5650-propagation-cursor-rank33-prefix'
if any(not (c / 'results' / (a + '.json')).exists() for a in ('candidate', 'kissat')):
    print('5650 pending'); raise SystemExit(0)
evidence = support['validate'](c)
m = json.loads((c / 'manifest.json').read_text())
result = dict(parent_full_replay=True, evidence=evidence, parent_summary_sha256=m['parent_summary_sha256'],
    limitation='Exact per-arm decisions,boundaries,formula/binary snapshots and partial proof bytes through1M;existing native182093 difference unchanged. No complete verdict or acceptance timing.')
p = c / 'summary.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: both complete1M parent traces and proof bytes identical')
