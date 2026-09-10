#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('summarize-sweep-rootfree-waves-6852.py')
code = source.read_text()
for before,after in {
    '20260910-6850-sweep-rootfree-runtime':'20260910-6864-subsolver-shrink-runtime',
    'solver-checkpoint-6852.json':'solver-checkpoint-6866.json',
    'Same-work Sweep root-free temporary-copy omission6838':'Same-work SubSolver shrink-only compaction6857',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
