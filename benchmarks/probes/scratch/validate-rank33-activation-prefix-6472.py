#!/usr/bin/env python3
"""Full streams with the maintained derived-counter accounting correction."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank45-stamp-prefix-6404.py')
code=source.read_text()
for old,new in {
 '20260909-6402-rank45-stamp-prefix':'20260909-6470-rank33-activation-prefix',
 'Rank45 prefix pending':'Rank33 prefix pending', 'rank=45':'rank=33',
 'from decision_prefix import read_prefix,first_stream_difference':'from decision_prefix import read_prefix,first_stream_difference,validate_stream_accounting',
 "    assert first[1]==(prefix['start_decisions']+1)%(1<<64)\n    assert prefix['start_decisions']==(prefix['end_decisions']-count)%(1<<64)":'    validate_stream_accounting(prefix,first,last,count)',
}.items():
    assert old in code,old; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
