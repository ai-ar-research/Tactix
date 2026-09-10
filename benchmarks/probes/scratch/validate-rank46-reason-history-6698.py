#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('validate-rank46-watch-history-6686.py')
code=source.read_text()
changes={
    '20260909-6684-rank46-watch-history':'20260909-6696-rank46-reason-history',
    'summary-6686.json':'summary-6698.json','108330':'108080','108341':'108170','250000':'500000','-16719':'-16365',
    "keys={a:dict(p.split('=',1) for p in rr[0]['payload'].split('|'))['set'] for a,rr in m['conflict_records'].items()}":"keys=m['reason_keys']"}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
