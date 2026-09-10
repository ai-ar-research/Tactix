#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('summarize-material-waves-6597.py')
code=source.read_text()
for before,after in {
 '20260909-6592-rank50-material-three-wave-runtime':'20260909-6741-rank46-watermark-runtime',
 'solver-checkpoint-6597.json':'solver-checkpoint-6745.json',
 "'(50,)'":"'(46,)'",
 "\"('pending_validation', 'checked_UNSAT')\":\"('pending_validation', 'checked_SAT')\",":"\"('pending_validation', 'checked_UNSAT')\":\"('pending_validation', 'checked_UNSAT')\",",
 'Changed-work material6579 versus6424 on Rank50':'Changed-work historical-watermark6730 versus6654 on Rank46'}.items():
    assert code.count(before)==1,before;code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
