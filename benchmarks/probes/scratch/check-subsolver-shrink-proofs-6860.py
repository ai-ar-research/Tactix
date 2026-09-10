#!/usr/bin/env python3
"""Fixed independent Sweep proof gate using the unchanged120-case emitter."""
from pathlib import Path
source = Path(__file__).with_name('check-sweep-rootfree-proofs-6845.py')
code = source.read_text()
for before,after in {
    '20260910-6845-sweep-rootfree-proofs':'20260910-6860-subsolver-shrink-proofs',
    '6838-sweep-rootfree':'6857-subsolver-shrink',
    'sweep-rootfree-6838':'subsolver-shrink-6857',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
