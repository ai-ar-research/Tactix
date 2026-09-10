#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('validate-rank46-allscan-reason-6704.py')
code=source.read_text()
changes={
    '20260909-6702-rank46-allscan-reason':'20260909-6721-rank46-earlier-allscan',
    'summary-6704.json':'summary-6723.json','108170':'107106','19005':'18623','-16365':'-18659',
    "m['reason_keys']['kissat']":"m['reason_keys']['candidate']"}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
