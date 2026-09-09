#!/usr/bin/env python3
"""Summarize all three stamp-repair waves with unchanged exact-work checks."""
from pathlib import Path
source = Path(__file__).with_name('summarize-collapse-waves-6343.py')
code = source.read_text()
for old, new in {
    '20260909-6332-collapse-three-wave-runtime': '20260909-6374-compact-stamp-three-wave-runtime',
    'solver-checkpoint-6343.json': 'solver-checkpoint-6382.json',
    '(32, 33, 40)': '(32, 33, 35)',
    'Changed-work candidate6305 versus6269': 'Changed-work candidate6357 versus6305',
}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
