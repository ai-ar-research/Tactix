#!/usr/bin/env python3
"""Conserve whole Factor envelopes with the previously replayed sample reader."""
from pathlib import Path

base = Path(__file__).with_name('extract-vivify-compact-wave-7153.py')
code = base.read_text()
for before, after in {
    'validate-vivify-compact-profiles-7152.py': 'validate-factor-arena-profiles-7187.py',
    '20260910-7153-vivify-compact-samples': '20260910-7188-factor-arena-samples',
    "'tactix::vivify_apply('": "'tactix::factor_apply_seeded('",
    "components.get('vivify', 0)": "components.get('factor', 0)",
    'complete Vivify cycles': 'complete Factor cycles',
    'complete_vivify_ratio=': 'complete_factor_ratio=',
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
