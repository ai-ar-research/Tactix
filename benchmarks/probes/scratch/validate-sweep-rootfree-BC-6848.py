#!/usr/bin/env python3
"""Strict exact-work Sweep checkpoint; final certificate outcomes stay separate."""
from pathlib import Path
source = Path(__file__).with_name('validate-vivify-snapshot-BC-6830.py')
code = source.read_text()
for before, after in {
    '20260910-6828-vivify-snapshot-BC-gate': '20260910-6846-sweep-rootfree-BC-gate',
    'summary-6830.json': 'summary-6848.json',
    'snapshot omission changed': 'Sweep root-free copy omission changed',
}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
