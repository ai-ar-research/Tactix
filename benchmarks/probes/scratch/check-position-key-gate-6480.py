#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('check-shared-blocker-gate-6453.py')
code=source.read_text()
for old,new in {
 '20260909-6451-shared-blocker-BC-gate':'20260909-6478-position-key-BC-gate',
 'solver-checkpoint-6453.json':'solver-checkpoint-6480.json',
}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
