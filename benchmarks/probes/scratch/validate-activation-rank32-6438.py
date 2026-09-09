#!/usr/bin/env python3
"""Use complete suffix/overlap validation with the inherited1M hash prerequisite."""
from pathlib import Path
source=Path(__file__).with_name('validate-collapse-prefix-6323.py')
code=source.read_text()
for old,new in {
 '20260909-6321-collapse-rank32-prefix':'20260909-6436-activation-rank32-prefix',
 'do not attribute the old1368117 split to6305':'do not claim the old3M closure for6424',
}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
