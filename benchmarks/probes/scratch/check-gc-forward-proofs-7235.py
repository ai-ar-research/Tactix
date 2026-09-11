#!/usr/bin/env python3
"""Use the repaired valid full-solver collector witnesses with gc-forward7233."""
from pathlib import Path
base = Path(__file__).with_name('check-root-binary-proofs-7219.py')
code = base.read_text()
for before,after in {
    '20260911-7219-root-binary-proofs':'20260911-7235-gc-forward-proofs',
    'test-root-binary-proof-7219.cpp':'test-root-binary-proof-7224.cpp',
    "('candidate','7218-root-binary','20260911')":"('candidate','7233-gc-forward','20260911')",
}.items():
    assert before in code, before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
