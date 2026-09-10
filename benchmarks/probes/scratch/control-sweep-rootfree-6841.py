#!/usr/bin/env python3
"""Fresh fixed-control artifacts after the compile-only fixture correction."""
from pathlib import Path
root = Path(__file__).resolve().parents[3]
assert (root/'benchmarks/runs/20260910-6838-sweep-rootfree-build/fixture-failure-6840.json').exists()
c = root/'benchmarks/runs/20260910-6841-sweep-rootfree-control'
c.mkdir()
(c/'logs').mkdir()
source = Path(__file__).with_name('control-sweep-rootfree-6839.py')
code = source.read_text()
for before, after in {
    '20260910-6838-sweep-rootfree-build': '20260910-6841-sweep-rootfree-control',
    'control-6839.json': 'control-6841.json',
}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
