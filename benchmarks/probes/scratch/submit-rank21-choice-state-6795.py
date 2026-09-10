#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-rank21-light-window-6775.py')
code = source.read_text()
old = '20260909-6774-rank21-light-window'
assert code.count(old) == 1
exec(compile(code.replace(old, '20260909-6794-rank21-choice-state'), str(source), 'exec'),
    {'__file__':str(source), '__name__':'__main__'})
