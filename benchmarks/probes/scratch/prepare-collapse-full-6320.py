#!/usr/bin/env python3
"""Evaluate6305 over all fixed50 after provisional B/C completion."""
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime as f
gate=root/'benchmarks/reports/2026-09-09-collapse-checkpoint-6319.json'
g=json.loads(gate.read_text())['campaigns']['20260909-6311-substitute-collapse-BC-gate']
assert g['solver_records']==66 and g['checked']>=64
c=root/'benchmarks/runs/20260909-6320-substitute-collapse-ranks1-50'
f.prepare(c,root/'benchmarks/runs/20260909-6305-substitute-collapse-build',
    root/'build/pin-6305-substitute-collapse-release/tactix',root/'build/pin-6305-substitute-collapse-bc/tactix')
m=json.loads((c/'manifest.json').read_text())
m['provisional_gate']={'path':str(gate.relative_to(root)),'sha256':f.sha(gate),'expected':66,'checked':64}
m['hypothesis']='Completing canonical Substitute binary-collapse retry may repair later search divergence; measure all50 without assuming a runtime improvement.'
m['decision']='Retain only with checked correctness and reference-backed semantics; runtime target all50 solved at <=1.01x fixed reference, plus independent memory/fuzz/repeated timing gates.'
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(c)
