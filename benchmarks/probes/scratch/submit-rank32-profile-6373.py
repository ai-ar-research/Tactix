#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-rank4-profile-6167.py')
code = source.read_text()
old = '20260909-6166-rank4-release-profile'
assert code.count(old) == 1
code = code.replace(old, '20260909-6372-rank32-release-profile')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
