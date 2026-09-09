#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('land-retry-full50-6260.py')
code=source.read_text()
for old,new in {
 '2026-09-09-retry-full50-audit-6259.json':'2026-09-09-position-key-audit-6481.json',
 'Record full Rank 1-50 repair outcome and remaining retry omissions':'Isolate narrower position-sort scratch with exact allocation and order controls',
}.items():
    assert code.count(old)==1; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
