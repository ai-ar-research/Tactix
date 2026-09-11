#!/usr/bin/env python3
"""Submit the exact-native memory diagnostic once; never refresh the score."""
from pathlib import Path
base = Path(__file__).with_name('submit-vivify-revalidation-full50-7167.py')
code = base.read_text()
before = '20260910-7167-vivify-revalidation-ranks1-50'
assert code.count(before) == 1
code = code.replace(before, '20260911-7315-rank25-native-memory')
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
