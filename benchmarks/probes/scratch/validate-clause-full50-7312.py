#!/usr/bin/env python3
"""Classify every full50 outcome and require exact own-parent raw work."""
from pathlib import Path
base = Path(__file__).with_name('validate-realloc-full50-7283.py')
code = base.read_text()
for before, after in {
    '20260911-7283-realloc-ranks1-50': '20260911-7312-clause-realloc-ranks1-50',
    '20260911-7262-occurrence-ranks1-50': '20260911-7283-realloc-ranks1-50',
    '2026-09-11-realloc-full50-7283': '2026-09-11-clause-realloc-full50-7312',
    'own7253': 'own7267',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'),
     {'__file__': str(Path(__file__)), '__name__': '__main__'})
