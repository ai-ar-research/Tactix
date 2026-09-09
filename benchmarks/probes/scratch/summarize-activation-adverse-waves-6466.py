#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('summarize-collapse-waves-6343.py')
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6464-activation-adverse-three-wave-runtime',
 'solver-checkpoint-6343.json':'solver-checkpoint-6466.json',
 '(32, 33, 40)':'(17, 33, 43)',
 "('pending_validation', 'checked_UNSAT')":"('pending_validation', 'checked_UNSAT', 'checked_SAT')",
 'Changed-work candidate6305 versus6269':'Changed-work activation6424 versus6383 on all three complete-table adverse selections',
}.items():
    assert old in code,old; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
