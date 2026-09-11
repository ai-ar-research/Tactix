#!/usr/bin/env python3
"""Strict two-task diagnostic profile gate, proof checks asynchronous."""
from pathlib import Path
base=Path(__file__).with_name('validate-vivify-compact-profiles-7152.py')
code=base.read_text()
for before,after in {
    '20260910-7152-vivify-compact-profiles':'20260911-7278-realloc-rank38-profiles',
    'solver-summary-7152.json':'solver-summary-7278.json',
    'submit-vivify-compact-wave-7150.py':'occurrence-fingerprint-7256.py',
    'h.fingerprint(':'h.raw_fingerprint(',
    'range(6)':'range(2)',
    'range(3)':'range(1)',
    "== '35'":"== '38'",
    '== 35':'== 38',
    "['35/'":"['38/'",
    '/6 exact':'/2 exact',
    '/6 checked':'/2 checked',
    "len(out['rows']) == 6":"len(out['rows']) == 2",
    'solver_outcomes=6':'solver_outcomes=2',
    'All three predeclared sampled pairs.':'The one predeclared diagnostic pair.',
    'Unprofiled7150 owns total-runtime and RSS gates.':'Unprofiled7273 owns repeated total-runtime and RSS gates.',
}.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(base),'exec'),globals())
