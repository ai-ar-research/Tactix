#!/usr/bin/env python3
"""Require full predecessor replay before interpreting selected-pair records."""
from pathlib import Path
source = Path(__file__).with_name('validate-rank50-material-6555.py')
code = source.read_text()
for old, new in {
    '20260909-6553-rank50-material-producers': '20260909-6556-rank50-material-pair',
    "target = c / 'summary-6555.json'": "target = c / 'summary-6558.json'",
    "old / 'summary.json'": "old / 'summary-6555.json'",
    "if n != 'kissat.material.tsv'": "if n != 'kissat.lifecycle.tsv'",
    "print(json.dumps(dict(events=rows, admissions=admissions), indent=2))":
        "print(json.dumps(dict(events=rows, pair=lifecycle, first_invitation=invitation), indent=2))",
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
anchor = "result = dict(hashes=hashes, prefix=prefix, material_events=rows, admissions=admissions,"
assert code.count(anchor) == 1
extra = '''from binary_lifecycle import read_lifecycle
with (c / 'traces/kissat.lifecycle.tsv').open() as stream:
    lifecycle = read_lifecycle(stream, 'kissat', {(-525, 574)})
assert lifecycle, 'selected pair was not observed'
pair_adds = [r for r in lifecycle if r['event'] == 'add' and r['conflicts'] == 780400]
assert len(pair_adds) == 1
assert pair_adds[0]['copies'] == 0
bind(root / 'tools/bench/binary_lifecycle.py')
bind(root / 'benchmarks/probes/scratch/validate-material-pair-6558.py')
'''
code = code.replace(anchor, extra + '\n' + anchor + '\n    pair_lifecycle=lifecycle,')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
