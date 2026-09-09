#!/usr/bin/env python3
"""Preserve the established3M closure before broader activation-repair scoring."""
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
gate=root/'benchmarks/runs/20260909-6427-activation-stamp-BC-gate/solver-checkpoint-6429.json'
if not gate.exists(): print('Complete6429 B/C solver gate required; no output created'); sys.exit(0)
assert json.loads(gate.read_text())['solver_records']==66
source=Path(__file__).with_name('prepare-collapse-prefix-6321.py')
code=source.read_text()
for old,new in {
 '20260909-6321-collapse-rank32-prefix':'20260909-6436-activation-rank32-prefix',
 '20260909-6305-substitute-collapse-build':'20260909-6424-activation-stamp-build',
 'build/pin-6305-substitute-collapse-':'build/pin-6424-activation-stamp-',
 'tactix-6321-':'tactix-6436-',
 '6321-collapse-prefix':'6436-activation-prefix',
 'Current6305':'Current6424',
 'current Substitute collapse repair':'native initial activation-stamp repair',
 'Retained endpoint retry can change later BVE scheduling; no assumed causal or performance benefit.':'Check whether corrected activation numbering preserves the earlier1M hash and entire native3M decision closure established by6357. If the earlier hash changes, localize earlier; do not claim preserved closure.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="m['resource_contract']['array']='0'"; assert code.count(anchor)==1
extra="""m['provisional_gate']={'path':str(gate.relative_to(root)),'sha256':sha(gate)}
closure=root/'benchmarks/runs/20260909-6365-compact-stamp-rank32-prefix/summary.json'
m['preserved_closure']={'path':str(closure.relative_to(root)),'sha256':sha(closure)}
"""
code=code.replace(anchor,extra+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','gate':gate})
