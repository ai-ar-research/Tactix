#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('land-retry-full50-6260.py')
code=source.read_text()
for old,new in (
    ('2026-09-09-retry-full50-audit-6259.json','2026-09-09-collapse-launch-audit-6325.json'),
    ('Record full Rank 1-50 repair outcome and remaining retry omissions','Launch current Substitute repair on fixed Rank 1-50 and cached Rank 32 replay')):
    assert old in code;code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
