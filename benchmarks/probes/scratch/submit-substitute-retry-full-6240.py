#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-learned-sort-full-6181.py')
code=source.read_text()
old='20260909-6181-learned-sort-ranks1-50'; assert code.count(old)==1
code=code.replace(old,'20260909-6239-substitute-retry-ranks1-50')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
