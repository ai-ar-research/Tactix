#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('land-retry-full50-6260.py')
code=source.read_text()
for before,after in {
    '2026-09-09-retry-full50-audit-6259.json':'2026-09-09-congruence-runtime-rank46-audit-6679.json',
    'Record full Rank 1-50 repair outcome and remaining retry omissions':'Confirm Rank49 timing and localize Rank46 conflict watch-order divergence',
}.items():
    assert code.count(before)==1
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
