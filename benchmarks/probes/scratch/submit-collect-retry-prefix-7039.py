#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-rank35-retry-producer-7025.py')
code = source.read_text()
before = '20260910-7024-rank35-retry-producer'
assert code.count(before) == 1
code = code.replace(before, '20260910-7038-collect-retry-prefix')
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
