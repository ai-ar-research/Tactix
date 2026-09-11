#!/usr/bin/env python3
"""Repair the fixture's missing inverse import permutation, retaining failures."""
from pathlib import Path
base = Path(__file__).with_name('check-root-binary-proofs-7219.py')
code = base.read_text()
for before,after in {
    '20260911-7219-root-binary-proofs':'20260911-7224-root-binary-proof-control',
    'test-root-binary-proof-7219.cpp':'test-root-binary-proof-7224.cpp',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
