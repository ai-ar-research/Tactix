#!/usr/bin/env python3
"""Freeze5830's first full solver view without overwriting any earlier snapshot."""
from pathlib import Path

here = Path(__file__).resolve().parent
path = here / 'snapshot-forward-runtime-5816.py'
program = path.read_text()
for old, new in {
    '20260908-5811-forward-unit-ranks1-40': '20260908-5838-forward-admission-ranks1-40',
    '20260908-5758-reuse-target-ranks1-40': '20260908-5811-forward-unit-ranks1-40',
    '20260908-5806-forward-unit-BC-gate': '20260908-5836-forward-admission-BC-gate',
}.items():
    assert old in program
    program = program.replace(old, new)
exec(compile(program, str(path), 'exec'), {'__file__': str(path), '__name__': '__main__'})
