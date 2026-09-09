#!/usr/bin/env python3
"""Correct6404's derived-start assumption; preserve its failed version."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank45-stamp-prefix-6404.py')
code=source.read_text()
old='from decision_prefix import read_prefix,first_stream_difference'
assert code.count(old)==1
code=code.replace(old,old+',validate_stream_accounting')
old="    assert first[1]==(prefix['start_decisions']+1)%(1<<64)\n    assert prefix['start_decisions']==(prefix['end_decisions']-count)%(1<<64)"
assert code.count(old)==1
code=code.replace(old,'    validate_stream_accounting(prefix,first,last,count)')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
