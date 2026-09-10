#!/usr/bin/env python3
"""Independent fixed proofs; all objects and headers belong to the same arm."""
from pathlib import Path
source = Path(__file__).with_name('check-sweep-rootfree-proofs-6845.py')
code = source.read_text()
for before, after in {
    '20260910-6845-sweep-rootfree-proofs':'20260910-6872-subsolver-binary-proofs',
    '6838-sweep-rootfree':'6869-subsolver-binary',
    'sweep-rootfree-6838':'subsolver-binary-6869',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
