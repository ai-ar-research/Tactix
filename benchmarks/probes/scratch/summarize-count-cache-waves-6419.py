#!/usr/bin/env python3
"""Three predeclared exact-work cache timing waves; no selective replication."""
from pathlib import Path
source=Path(__file__).with_name('summarize-collapse-waves-6343.py')
code=source.read_text()
for old,new in {
    '20260909-6332-collapse-three-wave-runtime':'20260909-6405-count-cache-three-wave-runtime',
    'solver-checkpoint-6343.json':'solver-checkpoint-6419.json',
    '(32, 33, 40)':'(32, 33, 35)',
    'Changed-work candidate6305 versus6269, not isolated unit cost or fixed50 competitiveness.':'Exact-work cache6383 versus6357; paired ratios estimate unit cost, not fixed50 competitiveness. Require both arm spreads<=2% before an absolute runtime claim.',
}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
