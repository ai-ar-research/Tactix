#!/usr/bin/env python3
"""Price all predeclared Congruence and whole-solver controls after18 exact solves."""
from pathlib import Path
import sys
source = Path(__file__).with_name('price-subsolver-waves-6887.py')
code = source.read_text()
for before,after in {
    '20260910-6864-subsolver-shrink-runtime':'20260910-6960-xor-presence-runtime',
    "',6866)":"',6962)",
    'timer-cost-6887.json':'timer-cost-6963.json',
    '(32,39,38)':'(47,42,38)',
    "('Sweep','BVE','Vivify')":"('Congruence','Sweep','Vivify','Substitute')",
}.items():
    assert before in code,before
    code = code.replace(before,after)
assert len(sys.argv) == 1
sys.argv.append('shrink')
code = code.replace('experiment=sys.argv[1]','experiment="xor-presence"')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
