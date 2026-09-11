#!/usr/bin/env python3
"""Validate complete current clause-arena external-memory outcomes."""
from pathlib import Path
base = Path(__file__).with_name('validate-proc-memory-7298.py')
code = base.read_text().replace('20260911-7298-proc-memory-rank31',
                              '20260911-7310-clause-memory-rank31')
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
