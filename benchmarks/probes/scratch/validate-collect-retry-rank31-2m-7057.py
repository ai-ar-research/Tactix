#!/usr/bin/env python3
"""Reuse7052's complete suffix/actual-overlap validator for the next interval."""
import hashlib
import json
from pathlib import Path
source = Path(__file__).with_name('validate-collect-retry-rank31-late-7052.py')
root = Path(__file__).resolve().parents[3]
authority = json.loads((root / 'benchmarks/runs/20260910-7050-collect-retry-rank31-late/summary-7052.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest() == authority['hashes'][str(source.relative_to(root))]
code = source.read_text()
replacements = {
    '20260910-7050-collect-retry-rank31-late': '20260910-7055-collect-retry-rank31-2m',
    "target = c / 'summary-7052.json'": "target = c / 'summary-7057.json'",
    "parent / 'summary-7048.json'": "parent / 'summary-7052.json'",
    "old['comparisons']['candidate_vs_previous']['decisions']['complete_equal']": "old['decisions']['complete_equal']",
    'inherited300k': 'inherited1m',
}
for before, after in replacements.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
