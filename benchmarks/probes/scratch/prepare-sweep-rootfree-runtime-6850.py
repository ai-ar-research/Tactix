#!/usr/bin/env python3
"""Predeclare exactly three Sweep-cost waves after the full B/C solver gate."""
from pathlib import Path
source = Path(__file__).with_name('prepare-vivify-snapshot-runtime-6831.py')
code = source.read_text()
for before, after in {
    '6831-vivify-snapshot-runtime': '6850-sweep-rootfree-runtime',
    '6828-vivify-snapshot-BC': '6846-sweep-rootfree-BC',
    'summary-6830.json': 'summary-6848.json',
    '6827-vivify-snapshot-proofs': '6845-sweep-rootfree-proofs',
    "['checked'] == 48": "['checked'] == 120",
    '6822-vivify-snapshot': '6838-sweep-rootfree',
    '20260910-6824-vivify-snapshot-control/control-6824.json': '20260910-6843-sweep-rootfree-control/control-6843.json',
    'tactix-vivify-snapshot-three-wave-runtime-v1': 'tactix-sweep-rootfree-three-wave-runtime-v1',
    'Omitting unused pure-irredundant non-flushing Vivify literal snapshots lowers implementation cost without altering work, budgets or CDCL. Price total solver and Vivify timers; retain all adverse outcomes and control38.':
        'Avoiding the intermediate copy for root-free Sweep environment clauses lowers implementation cost without altering work, budgets, scheduling or CDCL. Price total solver and Sweep timers; retain all adverse outcomes and low-slot control38. Independent of unadopted6822.',
    '113/113/112 full suites,432+216 actual old/new state/model/proof-digest cases,144 ordering cases and48 independent fixed proofs. All33 B/C pairs exact.':
        '113/113/112 full suites,300 actual old/new cases/1500 built environments with exact state/model/proof digests, existing full Sweep controls and120 independently checked fixed proofs. All33 B/C pairs exact.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
