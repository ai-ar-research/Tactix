#!/usr/bin/env python3
"""Successor snapshot after all 100 solver phases; preserve older tables."""
from pathlib import Path

source = Path(__file__).with_name('snapshot-fresh-rank50-6123.py')
code = source.read_text()
old = '2026-09-08-fresh-rank50-6123'
assert code.count(old) == 1
code = code.replace(old, '2026-09-08-fresh-rank50-6143')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
