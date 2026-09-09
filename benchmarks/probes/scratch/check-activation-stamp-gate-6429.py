#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('check-compact-stamp-gate-6364.py')
code=source.read_text()
for old,new in {'20260909-6362-compact-stamp-BC-gate':'20260909-6427-activation-stamp-BC-gate',
                'solver-checkpoint-6364.json':'solver-checkpoint-6429.json'}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
