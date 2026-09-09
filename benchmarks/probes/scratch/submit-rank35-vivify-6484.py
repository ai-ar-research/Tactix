#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-rank35-stamp-prefix-6377.py')
code=source.read_text()
for old,new in {
 '20260909-6376-rank35-stamp-prefix':'20260909-6483-rank35-vivify-replay',
 "m['prior_closure'],m['selection']":"m['prior_closure'],m['selection'],m['parent_summary']",
}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
