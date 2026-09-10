#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('validate-sweep-rootfree-BC-6848.py')
code = source.read_text()
for before,after in {
    '20260910-6846-sweep-rootfree-BC-gate':'20260910-6873-subsolver-binary-BC-gate',
    'summary-6848.json':'summary-6877.json',
    'Sweep root-free copy omission changed':'SubSolver binary-tagged watches changed',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
