#!/usr/bin/env python3
"""Read-only all-counter BC comparison; optional once-only fullwidth submit."""
from pathlib import Path
base = Path(__file__).with_name('check-submit-factor-arena-BC-7183.py')
code = base.read_text()
for before, after in {
    '20260910-7183-factor-arena-BC': '20260911-7272-dense-realloc-BC',
    'submit-vivify-compact-wave-7150.py': 'occurrence-fingerprint-7256.py',
    'h.fingerprint(': 'h.raw_fingerprint(',
    'Factor arena BC:': 'Dense realloc BC:',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
