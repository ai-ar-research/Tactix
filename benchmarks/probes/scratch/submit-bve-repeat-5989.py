#!/usr/bin/env python3
"""Submit only the newly frozen repeated-pop observer campaign."""
from pathlib import Path
source=Path(__file__).with_name('submit-forward-caller-5860.py')
code=source.read_text()
begin=code.index('names = {');end=code.index('\nfor name in names[',begin)
code=code[:begin]+"names = {'repeat': ['20260908-5988-bve-repeat-heap']}"+code[end:]
code=code.replace('Independent5849 successor','Diagnostic-only5985 observer;no timing reference change')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
