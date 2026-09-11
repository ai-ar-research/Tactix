#!/usr/bin/env python3
"""Measure complete forward-pass scopes before proposing a representation change."""
from pathlib import Path
base = Path(__file__).with_name('localize-factor-envelope-7171.py')
code = base.read_text()
for before, after in {
    '2026-09-10-factor-envelope-7171.json': '2026-09-11-forward-envelope-7247.json',
    "s.startswith('tactix::factor_apply_seeded(')": "s.startswith('tactix::strengthen_apply(')",
    "s == 'kissat_factor'": "s == 'kissat_forward_subsume_during_elimination'",
    "components.get('factor', 0)": "components.get('forward', 0)",
    'src/rules/factor/factor.cpp': 'src/rules/strengthen/strengthen.cpp',
    'kissat/src/factor.c': 'kissat/src/forward.c',
    'complete Factor envelope': 'complete forward-pass envelope',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
