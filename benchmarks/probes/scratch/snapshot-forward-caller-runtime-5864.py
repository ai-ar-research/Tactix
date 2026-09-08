#!/usr/bin/env python3
"""Freeze only a complete40 view; preserve all preceding checkpoints."""
from pathlib import Path

here = Path(__file__).resolve().parent
path = here / 'snapshot-forward-runtime-5816.py'
code = path.read_text()
for old, new in {
    '20260908-5811-forward-unit-ranks1-40': '20260908-5858-forward-caller-ranks1-40',
    '20260908-5758-reuse-target-ranks1-40': '20260908-5838-forward-admission-ranks1-40',
    '20260908-5806-forward-unit-BC-gate': '20260908-5855-forward-caller-BC-gate'}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(path), 'exec'), {'__file__': str(path), '__name__': '__main__'})
