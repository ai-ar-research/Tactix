#!/usr/bin/env python3
"""Submit the isolated successor using the unchanged checked task contract."""
from pathlib import Path

source = Path(__file__).with_name('submit-publication-memory-6130.py')
code = source.read_text()
old = '20260908-6129-publication-memory'
assert code.count(old) == 1
code = code.replace(old, '20260908-6148-collect-memory')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
