#!/usr/bin/env python3
"""Extend the validated rank4 prefix without executing a terminal proof run."""
from pathlib import Path
source = Path(__file__).with_name('prepare-priority-prefixes-6139.py')
code = source.read_text()
changes = {
    '((6139, 21, 10000), (6140, 4, 100000))': '((6160, 4, 650000),)',
    "f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'":
    "f'benchmarks/runs/20260909-{number}-rank{rank}-current-prefix'",
    'Rank4bothfreshUNSATcertificates pass;useashortcurrentprefix todistinguish changedwork fromunitcost beforeoptimizing.':
    'Rank4 current/native match169459 decisions through100K conflicts. Extend to650K,below both full651764 conflict totals,to discriminate a later search split before unit-cost optimization. This is not a full-solve identity claim.',
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
