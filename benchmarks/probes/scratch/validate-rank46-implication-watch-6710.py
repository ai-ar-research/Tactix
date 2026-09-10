#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('validate-rank46-reason-watch-6695.py')
code=source.read_text()
for before,after in {'20260909-6693-rank46-reason-watch':'20260909-6708-rank46-implication-watch','summary-6695.json':'summary-6710.json'}.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
