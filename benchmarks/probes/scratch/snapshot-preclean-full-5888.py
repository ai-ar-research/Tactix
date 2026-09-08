#!/usr/bin/env python3
"""Seal only a complete preclean full40 solver view."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-forward-runtime-5816.py')
code = source.read_text()
for old, new in {
    '20260908-5811-forward-unit-ranks1-40': '20260908-5880-forward-preclean-ranks1-40',
    '20260908-5758-reuse-target-ranks1-40': '20260908-5858-forward-caller-ranks1-40',
    '20260908-5806-forward-unit-BC-gate': '20260908-5878-forward-preclean-BC-gate',
}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
