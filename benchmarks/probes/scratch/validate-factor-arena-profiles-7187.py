#!/usr/bin/env python3
"""Frozen profile validator retargeted without changing its acceptance rules."""
from pathlib import Path

base = Path(__file__).with_name('validate-vivify-compact-profiles-7152.py')
code = base.read_text()
for before, after in {
    '20260910-7152-vivify-compact-profiles': '20260910-7187-factor-arena-profiles',
    'solver-summary-7152.json': 'solver-summary-7187.json',
    'Unprofiled7150 owns total-runtime and RSS gates.': 'Unprofiled7186 owns total-runtime and RSS gates.',
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), globals())
