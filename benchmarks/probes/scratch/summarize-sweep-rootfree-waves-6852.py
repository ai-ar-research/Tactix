#!/usr/bin/env python3
"""Strict eighteen-task same-work Sweep timing summary; no reference refresh."""
from pathlib import Path
source = Path(__file__).with_name('summarize-vivify-snapshot-waves-6833.py')
code = source.read_text()
for before, after in {
    '20260910-6831-vivify-snapshot-runtime': '20260910-6850-sweep-rootfree-runtime',
    'solver-checkpoint-6833.json': 'solver-checkpoint-6852.json',
    'Same-work snapshot omission6822': 'Same-work Sweep root-free temporary-copy omission6838',
}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
