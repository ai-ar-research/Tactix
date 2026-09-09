#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-compact-stamp-BC-6363.py')
code = source.read_text()
old = '20260909-6362-compact-stamp-BC-gate'
assert code.count(old) == 1
exec(compile(code.replace(old, '20260909-6584-material-schedule-BC-gate'), str(source), 'exec'),
     {'__file__': str(source), '__name__': '__main__'})
