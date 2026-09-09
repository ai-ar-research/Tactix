#!/usr/bin/env python3
"""Reuse tested contiguous-stream comparison with explicit Rank37 bindings."""
from pathlib import Path
source = Path(__file__).with_name('compare-forward-admission-rank32-5839.py')
code = source.read_text()
changes = {
    '20260908-5837-forward-admission-rank32-prefix': '20260908-6070-root-lock-rank37-prefix',
    "prefix['conflict'] == native_prefix['conflict'] == 300000": "prefix['conflict'] == native_prefix['conflict'] == 1000000",
    "prefix['common_hash'] == native_prefix['common_hash']": "prefix['common_hash'] == early_prefix['hash']",
    "row['conflicts'] == '179463'": "row['conflicts'] == '188501'",
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
