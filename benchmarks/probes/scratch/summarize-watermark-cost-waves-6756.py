#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('summarize-material-waves-6597.py')
code=source.read_text()
for before,after in {
 '20260909-6592-rank50-material-three-wave-runtime':'20260909-6754-rank38-watermark-cost-runtime',
 'solver-checkpoint-6597.json':'solver-checkpoint-6756.json',
 "'(50,)'":"'(38,)'",
 "\"('pending_validation', 'checked_UNSAT')\":\"('pending_validation', 'checked_SAT')\",":"\"('pending_validation', 'checked_UNSAT')\":\"('pending_validation', 'checked_UNSAT')\",",
 "if r['arm'] == 'previous':":"if True: # both arms must preserve the same frozen work/certificate",
 'Changed-work material6579 versus6424 on Rank50':'Exact-work watermark6730 versus6654 on Rank38'}.items():
    assert code.count(before)==1,before;code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
