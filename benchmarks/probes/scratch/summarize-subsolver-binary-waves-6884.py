#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('summarize-subsolver-shrink-waves-6866.py')
code = source.read_text()
for before,after in {
    '20260910-6864-subsolver-shrink-runtime':'20260910-6882-subsolver-binary-runtime',
    'solver-checkpoint-6866.json':'solver-checkpoint-6884.json',
    'Same-work SubSolver shrink-only compaction6857':'Same-work approved full-width binary-tagged SubSolver watches6869',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
