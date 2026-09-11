#!/usr/bin/env python3
from pathlib import Path
base = Path(__file__).with_name('submit-rank31-bve-priority-7074.py')
code = base.read_text()
before = '20260910-7073-rank31-bve-priority'
assert code.count(before) == 1
code = code.replace(before, '20260911-7210-rank31-retry-producer')
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
