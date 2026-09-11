#!/usr/bin/env python3
"""Read-only comparison by default, full-width single-wave submission on request."""
from pathlib import Path
base = Path(__file__).with_name('check-submit-factor-arena-runtime-7186.py')
code = base.read_text()
for before, after in {
    '20260910-7186-factor-arena-runtime':'20260911-7257-occurrence-runtime',
    'check-submit-factor-arena-BC-7183.py':'check-submit-occurrence-BC-7256.py',
    'Factor runtime':'Occurrence runtime',
    'h.fingerprint(r) == h.fingerprint(records[task])':'h.raw_fingerprint(r) == h.raw_fingerprint(records[task])',
    'first and h.fingerprint(r) == h.fingerprint(first)':'first and h.raw_fingerprint(r) == h.raw_fingerprint(first)',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
