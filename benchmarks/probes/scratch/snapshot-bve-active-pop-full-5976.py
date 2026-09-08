#!/usr/bin/env python3
"""Freeze5962 full40 once, decomposing work versus the fixed native vector."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-forward-runtime-5816.py')
code = source.read_text()
for old, new in {
    '20260908-5811-forward-unit-ranks1-40': '20260908-5970-bve-active-pop-ranks1-40',
    '20260908-5758-reuse-target-ranks1-40': '20260908-5908-reduce-root-ranks1-40',
    '20260908-5806-forward-unit-BC-gate': '20260908-5968-bve-active-pop-BC-gate',
}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
