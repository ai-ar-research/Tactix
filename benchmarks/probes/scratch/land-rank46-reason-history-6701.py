#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('land-retry-full50-6260.py')
code=source.read_text()
for before,after in {
    '2026-09-09-retry-full50-audit-6259.json':'2026-09-09-rank46-reason-history-audit-6700.json',
    'Record full Rank 1-50 repair outcome and remaining retry omissions':'Validate Rank46 unit-reason ordering and trace its earlier history',
}.items():
    assert code.count(before)==1
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
