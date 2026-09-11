#!/usr/bin/env python3
"""Preserve7219 failure and run its diagnostic successor against both arms."""
from pathlib import Path
base = Path(__file__).with_name('check-root-binary-proofs-7219.py')
code = base.read_text()
for before,after in {
    '20260911-7219-root-binary-proofs':'20260911-7222-root-binary-proof-control',
    'test-root-binary-proof-7219.cpp':'test-root-binary-proof-7222.cpp',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
