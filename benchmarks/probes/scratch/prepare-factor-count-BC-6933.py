#!/usr/bin/env python3
"""Freeze unchanged B28+C5 for isolated Factor counter dataflow."""
from pathlib import Path
import json
root = Path(__file__).resolve().parents[3]
proof = json.loads((root/'benchmarks/runs/20260910-6932-factor-count-proofs/summary.json').read_text())
assert proof['checked'] == 288 and proof['negative_controls'] == 2
source = Path(__file__).with_name('prepare-vivify-snapshot-BC-6828.py')
code = source.read_text()
for before, after in {
    '6822-vivify-snapshot':'6925-factor-count',
    '6828-vivify-snapshot-BC':'6933-factor-count-BC',
    '6827-vivify-snapshot-proofs':'6932-factor-count-proofs',
    '20260910-6824-vivify-snapshot-control/control-6824.json':'20260910-6925-factor-count-build/control-6928.json',
    'Same-work Vivify snapshot omission on current6730':'Same-work Factor counter dataflow on current6730',
    'Avoid copying unused literal snapshots only in pure-irredundant non-flushing schedules.':
        'Save the cofactor reference/value across the separate touched-vector push, preserving publication-before-increment, exact candidate discovery order and all predicates/ticks/budgets/API/proofs/CDCL.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
