#!/usr/bin/env python3
"""Price all50 after the B/C guard and the motivating/preserved trace closures."""
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime as f
gate=root/'benchmarks/runs/20260909-6427-activation-stamp-BC-gate/solver-checkpoint-6429.json'
closures={str(rank):root/('benchmarks/runs/20260909-'+name+'/summary.json') for rank,name in (
    (45,'6433-activation-rank45-prefix'),(32,'6436-activation-rank32-prefix'))}
if not gate.exists() or not all(p.exists() for p in closures.values()):
    print('Complete B/C and both trace closures required; no campaign created'); sys.exit(0)
g=json.loads(gate.read_text()); assert g['solver_records']==66
control={r:json.loads(p.read_text()) for r,p in closures.items()}
assert control['45']['comparison_native']['complete_equal']
assert control['45']['comparison_native']['matching_rows']==184622
assert [r['drop'] for r in control['45']['transitive76464']]==[32,5]
assert control['32']['decisions']['complete_equal']
assert control['32']['matching_decisions_from_start']==153176971
for data in control.values():
    for rel,digest in data['hashes'].items(): assert f.sha(root/rel)==digest
c=root/'benchmarks/runs/20260909-6439-activation-ranks1-50'
f.prepare(c,root/'benchmarks/runs/20260909-6424-activation-stamp-build',
    root/'build/pin-6424-activation-stamp-release/tactix',root/'build/pin-6424-activation-stamp-bc/tactix')
m=json.loads((c/'manifest.json').read_text())
m['provisional_gate']=dict(path=str(gate.relative_to(root)),sha256=f.sha(gate),expected=66,checked=g['checked'])
m['causal_closures']={r:dict(path=str(p.relative_to(root)),sha256=f.sha(p)) for r,p in closures.items()}
m['hypothesis']='Native first-activation numbering removes stale fixed-key offsets while retaining Rank32 compact-stamp closure. Price all50 search/work/runtime/memory effects, not just Rank45; no speedup assumed.'
m['decision']='All50 fixed matched12CPU reference and every discrepancy remain scored. Pending certificates allow development, not acceptance or promotion; memory, latest-source fuzz and repeated timing remain independent.'
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n'); print(c)
