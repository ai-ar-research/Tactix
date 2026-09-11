#!/usr/bin/env python3
"""Submit a single exact-work arena profile pair using the frozen protocol."""
from pathlib import Path

base = Path(__file__).with_name('submit-vivify-compact-profile-wave-7152.py')
code = base.read_text()
before = 'validate-vivify-compact-profiles-7152.py'
assert code.count(before) == 1
code = code.replace(before, 'validate-factor-arena-profiles-7187.py')
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
