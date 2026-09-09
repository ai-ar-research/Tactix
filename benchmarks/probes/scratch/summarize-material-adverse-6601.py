#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('summarize-collapse-waves-6343.py')
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6599-material-adverse-rank38',
 'solver-checkpoint-6343.json':'solver-checkpoint-6601.json',
 'range(18)':'range(6)',
 'range(6*wave, 6*wave+6)':'range(2*wave, 2*wave+2)',
 'matrix[6*wave:6*wave+6]':'matrix[2*wave:2*wave+2]',
 '(32, 33, 40)':'(38,)',
 't // 6 == wave':'t // 2 == wave',
 'solver_outcomes=18':'solver_outcomes=6',
 'Changed-work candidate6305 versus6269':'Changed-work material6579 versus6424 on all of the complete-table adverse selection, Rank38',
}.items():
    assert old in code,old
    code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
