#!/usr/bin/env python3
"""Price all predeclared Factor and whole-binary controls after18 exact results."""
from pathlib import Path
source = Path(__file__).with_name('price-subsolver-waves-6887.py')
code = source.read_text()
for before, after in {
    '20260910-6864-subsolver-shrink-runtime':'20260910-6936-factor-count-runtime',
    "',6866)":"',6938)",
    'timer-cost-6887.json':'timer-cost-6939.json',
    "('Sweep','BVE','Vivify')":"('Factor','Sweep','BVE','Vivify','Substitute')",
}.items():
    assert before in code, before
    code = code.replace(before, after)
import sys
assert len(sys.argv) == 1
sys.argv.append('shrink')
code = code.replace('experiment=sys.argv[1]','experiment="factor-count"')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
