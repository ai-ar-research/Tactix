#!/usr/bin/env python3
"""Reuse B28+C5 after exact actual predecessor/candidate fixture controls."""
from pathlib import Path
source=Path(__file__).with_name('prepare-substitute-retry-gates-6236.py')
code=source.read_text(); marker="\nold = root/'benchmarks/runs/20260909-6183-rank32-current-prefix'"
assert code.count(marker)==1; code=code.split(marker)[0]
for old,new in {
 '20260909-6231-substitute-retry-build':'20260909-6448-shared-blocker-build',
 "assert (root/'benchmarks/runs/20260909-6234-substitute-retry-references/summary.json').exists()":"control=authority/'control-6450.json'\nassert control.exists()\ncontrols=json.loads(control.read_text())\nassert controls['outputs']['candidate']==controls['outputs']['previous']\nfor rel,digest in controls['hashes'].items(): assert sha(root/rel)==digest",
 '20260909-6236-substitute-retry-BC-gate':'20260909-6451-shared-blocker-BC-gate',
 'build/pin-6231-substitute-retry-':'build/pin-6448-shared-blocker-',
 'canonical Substitute removal publication':'shared propagation blocker fast path',
 'Reference-backed retry repair may change search work. No exact parent work or speedup assumed.':'Behavior-preserving watch-loop branch factoring must preserve every6424 proof byte and non-timing statistic. All33 candidate pairs and original production guard retained; no speedup assumed. Source, public surface, ticks and schedule invariants remain unchanged.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="m['pin_sha256']="; assert code.count(anchor)==1
code=code.replace(anchor,"m['actual_parent_control']={'path':str(control.relative_to(root)),'sha256':sha(control)}\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
