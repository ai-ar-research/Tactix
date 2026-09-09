#!/usr/bin/env python3
"""Prepare unchanged B28+C5 after actual-object ordering/allocation controls."""
from pathlib import Path
source=Path(__file__).with_name('prepare-substitute-retry-gates-6236.py')
code=source.read_text(); marker="\nold = root/'benchmarks/runs/20260909-6183-rank32-current-prefix'"
assert code.count(marker)==1; code=code.split(marker)[0]
for old,new in {
 '20260909-6231-substitute-retry-build':'20260909-6475-position-key-build',
 "assert (root/'benchmarks/runs/20260909-6234-substitute-retry-references/summary.json').exists()":"control=authority/'control-6477.json'\ncontrols=json.loads(control.read_text())\nx,y=controls['measurements']['candidate'],controls['measurements']['previous']\nfor key in ('cases','sorted_cases','sorted_refs','digest'): assert x[key]==y[key]\nassert x['cases']==480 and x['allocated_bytes']==12*x['sorted_refs'] and y['allocated_bytes']==20*y['sorted_refs']\nfor rel,digest in controls['hashes'].items(): assert sha(root/rel)==digest",
 '20260909-6236-substitute-retry-BC-gate':'20260909-6478-position-key-BC-gate',
 'build/pin-6231-substitute-retry-':'build/pin-6475-position-key-',
 'canonical Substitute removal publication':'temporary clause-position key narrowing',
 'Reference-backed retry repair may change search work. No exact parent work or speedup assumed.':'Temporary key-width reduction must preserve every6424 proof byte and non-timing statistic. All33 candidate pairs and the production guard remain; no fullsolver RSS/runtime gain assumed. Only sort scratch changes, not storage layout, public surface, ticks or schedule.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="m['pin_sha256']="; assert code.count(anchor)==1
code=code.replace(anchor,"m['actual_parent_control']={'path':str(control.relative_to(root)),'sha256':sha(control)}\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
