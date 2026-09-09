#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-compact-stamp-BC-6363.py')
code = source.read_text()
old = '20260909-6362-compact-stamp-BC-gate'
assert code.count(old) == 1
code = code.replace(old, '20260909-6387-vivify-count-cache-BC-gate')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
