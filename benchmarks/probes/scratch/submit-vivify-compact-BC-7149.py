#!/usr/bin/env python3
"""Submit compact Vivify7149 exactly once without touching prior campaigns."""
from pathlib import Path
template = Path(__file__).with_name('submit-compact-stamp-BC-6363.py')
code = template.read_text()
before = '20260909-6362-compact-stamp-BC-gate'
assert code.count(before) == 1
code = code.replace(before, '20260910-7149-vivify-compact-BC-gate')
exec(compile(code, str(template), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
