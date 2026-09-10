#!/usr/bin/env python3
"""Isolate ternary XOR membership reuse on6730, not on pending Factor6925."""
from pathlib import Path
source = Path(__file__).with_name('prepare-subsolver-shrink-6857.py')
code = source.read_text()
for before, after in {
    'runtime-subsolver-shrink-6857':'runtime-xor-presence-6950',
    '20260910-6857-subsolver-shrink-build':'20260910-6950-xor-presence-build',
}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
