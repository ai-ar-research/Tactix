#!/usr/bin/env python3
"""Apply the conserved envelope reader to the larger retained Congruence gap."""
from pathlib import Path

base = Path(__file__).with_name('localize-factor-envelope-7171.py')
code = base.read_text()
for before, after in {
    '2026-09-10-factor-envelope-7171.json': '2026-09-11-congruence-envelope-7193.json',
    "'tactix::factor_apply_seeded('": "'tactix::congruence_apply('",
    "'kissat_factor'": "'kissat_congruence'",
    "components.get('factor', 0)": "components.get('congruence', 0)",
    'runtime-vivify-compact-7144/src/rules/factor/factor.cpp': 'runtime-vivify-compact-7144/src/rules/congruence/congruence.cpp',
    'kissat/src/factor.c': 'kissat/src/congruence.c',
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
