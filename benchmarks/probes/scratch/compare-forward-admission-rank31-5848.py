#!/usr/bin/env python3
"""Reuse the complete, hash-checked Rank31 comparator for candidate5830."""
from pathlib import Path

root = Path(__file__).resolve().parents[3]
source = root / 'benchmarks/probes/scratch/compare-forward-unit-rank31-5814.py'
code = source.read_text().replace(
    '20260908-5810-forward-unit-rank31-prefix',
    '20260908-5845-forward-admission-rank31-prefix')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
