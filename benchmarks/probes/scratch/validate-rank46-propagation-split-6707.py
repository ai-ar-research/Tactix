#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('validate-rank46-search-window-6678.py')
code=source.read_text()
for before,after in {'20260909-6676-rank46-search-window':'20260909-6705-rank46-propagation-split',
                     'summary-6678.json':'summary-6707.json','108330..108341':'107100..107106'}.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
