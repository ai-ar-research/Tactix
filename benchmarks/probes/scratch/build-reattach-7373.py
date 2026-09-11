#!/usr/bin/env python3
"""Full successor builds; run outside ptrace for unsuppressed leak checks."""
from pathlib import Path
base=Path(__file__).with_name('build-lucky-roots-7366.py')
code=base.read_text()
for before,after in {
    'runtime-lucky-roots-7365':'runtime-lucky-reattach-7371',
    '7366-lucky-roots':'7373-lucky-reattach',
    "fixtures = ('lucky_roots', 'lucky', 'root_fold'":"fixtures = ('heuristics_reattach', 'lucky_roots', 'lucky', 'root_fold'",
    "count = 123 if kind == 'trace' else 124":"count = 124 if kind == 'trace' else 125",
    'sixteen fixed':'seventeen fixed',
}.items():
    assert before in code,before;code=code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__'))
