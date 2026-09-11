#!/usr/bin/env python3
from pathlib import Path
base = Path(__file__).with_name('check-submit-factor-arena-runtime-7186.py')
code = base.read_text()
for before, after in {
    '20260910-7186-factor-arena-runtime': '20260911-7212-factor-arena-transfer-controls',
    "('pending_validation', 'checked_UNSAT')": "('pending_validation', 'checked_SAT')",
    'Factor runtime': 'Factor transfer controls',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
