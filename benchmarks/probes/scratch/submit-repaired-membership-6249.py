#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-rank32-extension-6184.py')
code=source.read_text()
old='20260909-6183-rank32-current-prefix'; assert code.count(old)==1
code=code.replace(old,'20260909-6248-repaired-rank32-membership')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
