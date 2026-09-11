#!/usr/bin/env python3
"""Use unchanged whole-work and memory-interval gates for the lighter observer."""
from pathlib import Path
base = Path(__file__).with_name('validate-realloc-rank31-memory-7277.py')
code = base.read_text()
before = '20260911-7277-realloc-rank31-memory'
assert code.count(before) == 1
code = code.replace(before, '20260911-7286-memory-only-rank31')
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
