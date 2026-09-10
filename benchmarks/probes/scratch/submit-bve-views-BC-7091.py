#!/usr/bin/env python3
"""Submit the isolated full-width gate once; never edit its predecessor."""
from pathlib import Path
source = Path(__file__).with_name('submit-compact-stamp-BC-6363.py')
code = source.read_text()
before = '20260909-6362-compact-stamp-BC-gate'
assert code.count(before) == 1
code = code.replace(before, '20260910-7091-bve-views-BC-gate')
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
