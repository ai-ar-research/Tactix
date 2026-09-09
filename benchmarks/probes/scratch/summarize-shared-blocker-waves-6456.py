#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('summarize-collapse-waves-6343.py')
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6454-shared-blocker-three-wave-runtime',
 'solver-checkpoint-6343.json':'solver-checkpoint-6456.json',
 '(32, 33, 40)':'(32, 35, 45)',
 'Changed-work candidate6305 versus6269, not isolated unit cost or fixed50 competitiveness.':'Exact-work shared-blocker6448 versus6424 estimates execution cost, not fixed50 competitiveness. All three pairs and each arm spread required; no selective extra waves.',
}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
