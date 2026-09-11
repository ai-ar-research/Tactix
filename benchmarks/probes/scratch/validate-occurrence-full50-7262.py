#!/usr/bin/env python3
"""Read-only exact all50 gate by default; a once-only seal retains every rank."""
from pathlib import Path
base = Path(__file__).with_name('validate-vivify-revalidation-full50-7168.py')
code = base.read_text()
changes = {
    'submit-vivify-compact-wave-7150.py':'occurrence-fingerprint-7256.py',
    '20260910-7167-vivify-revalidation-ranks1-50':'20260911-7262-occurrence-ranks1-50',
    '20260910-7053-collect-retry-ranks1-50':'20260910-7167-vivify-revalidation-ranks1-50',
    '2026-09-10-vivify-revalidation-full50-7168':'2026-09-11-occurrence-full50-7262',
    'h.fingerprint(r) == h.fingerprint(records[task])':
        'h.raw_fingerprint(r) == h.raw_fingerprint(records[task])',
    'One fresh full-set transfer view against fixed native and checked7030;':
        'One fresh single-image memory/runtime transfer against fixed native and checked7144. Only strictly parsed dense move/defrag physical counters and timing excluded across arms; same-record physical counters exact;',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
