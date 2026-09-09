#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-compact-stamp-followup-6367.py')
code=source.read_text()
old="names = {'prefix':'20260909-6365-compact-stamp-rank32-prefix',\n         'full':'20260909-6366-compact-stamp-ranks1-50'}"
assert code.count(old)==1
code=code.replace(old,"names = {'full':'20260909-6407-count-cache-ranks1-50'}")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
