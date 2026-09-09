#!/usr/bin/env python3
"""Use the unchanged fixed50 tool through a frozen build-authority adapter."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
import fixed_runtime as f
timing = root / 'benchmarks/runs/20260909-6176-learned-sort-rank4-runtime'
tp = timing / ('summary.json' if (timing / 'summary.json').exists() else 'solver-checkpoint.json')
t = json.loads(tp.read_text())
assert t['solver_results'] == 6 and t['exact_predecessor_work_and_proof']
assert t['paired']['max'] < 1, 'all predeclared pairs must improve before full-view measurement'
gate = root / 'benchmarks/runs/20260909-6171-learned-sort-BC-gate'
gp = gate / ('summary.json' if (gate / 'summary.json').exists() else 'solver-checkpoint.json')
g = json.loads(gp.read_text())
assert g['solver_results'] == 66 and g['exact_predecessor_work_and_proof'] == 66
assert all(v < 1 for v in g['candidate_over_production'].values())
build = root / 'benchmarks/runs/20260909-6169-learned-sort-build'
b = json.loads((build / 'manifest.json').read_text())
authority = root / 'benchmarks/runs/20260909-6180-learned-sort-full-authority'
assert not authority.exists()
authority.mkdir()
shutil.copy2(build / 'cumulative.patch', authority / 'cumulative.patch')
# The established preparation API names the ancestor commit 'production'.
# Supply that spelling in a successor, never rewrite the frozen build manifest.
a = dict(b, production=b['source_commit'],
    build_authority=dict(path=str((build / 'manifest.json').relative_to(root)), sha256=f.sha(build / 'manifest.json')),
    predecessor_timing=dict(path=str(tp.relative_to(root)), sha256=f.sha(tp)),
    bc_gate=dict(path=str(gp.relative_to(root)), sha256=f.sha(gp)))
(authority / 'manifest.json').write_text(json.dumps(a, indent=2) + '\n')
full = root / 'benchmarks/runs/20260909-6181-learned-sort-ranks1-50'
f.prepare(full, authority, root / 'build/pin-6169-learned-sort-release/tactix',
          root / 'build/pin-6170-learned-sort-bc/tactix')
print(full)
