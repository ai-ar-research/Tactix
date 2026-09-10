#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-sweep-rootfree-wave-6851.py')
code = source.read_text()
for before,after in {
    '20260910-6846-sweep-rootfree-BC-gate':'20260910-6861-subsolver-shrink-BC-gate',
    '20260910-6850-sweep-rootfree-runtime':'20260910-6864-subsolver-shrink-runtime',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
