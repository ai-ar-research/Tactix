#!/usr/bin/env python3
"""Submit only the new isolated BC campaign; preserve every previous runner."""
from pathlib import Path
source = Path(__file__).with_name('submit-rank4-profile-6167.py')
code = source.read_text()
old = '20260909-6166-rank4-release-profile'
assert code.count(old) == 1
code = code.replace(old, '20260909-6171-learned-sort-BC-gate')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
