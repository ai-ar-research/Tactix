#!/usr/bin/env python3
"""Require exact own-arm work, including the physical dense storage counters."""
from pathlib import Path
base = Path(__file__).with_name('validate-vivify-compact-profiles-7152.py')
code = base.read_text()
for before, after in {
    '20260910-7152-vivify-compact-profiles':'20260911-7259-occurrence-profiles',
    'solver-summary-7152.json':'solver-summary-7259.json',
    'submit-vivify-compact-wave-7150.py':'occurrence-fingerprint-7256.py',
    'h.fingerprint(':'h.raw_fingerprint(',
    'Unprofiled7150 owns total-runtime and RSS gates.':'Unprofiled7257 owns total-runtime and RSS gates.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),globals())
