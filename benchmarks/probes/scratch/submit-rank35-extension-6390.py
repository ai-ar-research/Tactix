#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-rank35-stamp-prefix-6377.py')
code=source.read_text()
old='20260909-6376-rank35-stamp-prefix'
assert code.count(old)==1
code=code.replace(old,'20260909-6389-rank35-stamp-extension')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
