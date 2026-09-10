#!/usr/bin/env python3
"""User explicitly resumed after the old cutoff; submit this fresh gate once."""
from pathlib import Path
source = Path(__file__).with_name('submit-compact-stamp-BC-6363.py')
code = source.read_text()
before = '20260909-6362-compact-stamp-BC-gate'
assert code.count(before) == 1
code = code.replace(before, '20260910-7034-collect-retry-BC-gate')
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
