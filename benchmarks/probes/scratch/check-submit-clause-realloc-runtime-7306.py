#!/usr/bin/env python3
"""Strict30-task all-counter reader or once-only fullwidth ten-task wave."""
from pathlib import Path
base = Path(__file__).with_name('check-submit-dense-realloc-runtime-7273.py')
code = base.read_text()
for before, after in {
    '20260911-7273-dense-realloc-runtime': '20260911-7306-clause-realloc-runtime',
    'check-submit-dense-realloc-BC-7272.py': 'check-submit-clause-realloc-BC-7305.py',
    'Dense realloc': 'Clause realloc',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
