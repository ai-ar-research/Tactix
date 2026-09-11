#!/usr/bin/env python3
"""Partition the larger Reduce envelope using only frozen full-run streams."""
from pathlib import Path
base = Path(__file__).with_name('localize-factor-envelope-7171.py')
code = base.read_text()
for before, after in {
    '2026-09-10-factor-envelope-7171.json': '2026-09-11-reduce-envelope-7216.json',
    "'tactix::factor_apply_seeded('": "'tactix::reduce_apply_refs('",
    "'kissat_factor'": "'kissat_reduce'",
    "components.get('factor', 0)": "components.get('reduce', 0)",
    'runtime-vivify-compact-7144/src/rules/factor/factor.cpp':
        'runtime-vivify-compact-7144/src/rules/reduce/reduce.cpp',
    'kissat/src/factor.c': 'kissat/src/reduce.c',
    'complete Factor envelope': 'complete Reduce-apply envelope',
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
