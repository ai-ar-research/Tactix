#!/usr/bin/env python3
"""Full fixed50 pricing after a complete provisional B/C gate."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
import fixed_runtime as f
gate = root/'benchmarks/runs/20260909-6362-compact-stamp-BC-gate/solver-checkpoint-6364.json'
g = json.loads(gate.read_text())
assert g['solver_records'] == 66
c = root/'benchmarks/runs/20260909-6366-compact-stamp-ranks1-50'
f.prepare(c, root/'benchmarks/runs/20260909-6357-compact-stamp-build',
    root/'build/pin-6357-compact-stamp-release/tactix', root/'build/pin-6357-compact-stamp-bc/tactix')
m = json.loads((c/'manifest.json').read_text())
m['provisional_gate'] = dict(path=str(gate.relative_to(root)), sha256=f.sha(gate), expected=66, checked=g['checked'])
m['hypothesis'] = 'Faithful detached queue-state preservation repairs a measured source-order key mismatch. Price all50 without assuming a runtime benefit or identical work.'
m['decision'] = 'All50 remain scored against the fixed matched12CPU reference, including matched41–50. Correctness, memory, latest-source fuzz and causal repeated timing remain independent gates.'
(c/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
print(c)
