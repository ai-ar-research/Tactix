#!/usr/bin/env python3
"""Freeze B28+C5 exact-work gate for the independently tested ITE hoist."""
from pathlib import Path
import json
root = Path(__file__).resolve().parents[3]
proof = json.loads((root/'benchmarks/runs/20260910-6990-ite-pair-proofs/summary.json').read_text())
assert proof['checked'] == 384 and proof['negative_controls'] == 2
assert (root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build/budget-control-6988.json').exists()
source = Path(__file__).with_name('prepare-vivify-snapshot-BC-6828.py')
code = source.read_text()
for before, after in {
    '6822-vivify-snapshot': '6969-ite-pair-hoist',
    '6828-vivify-snapshot-BC': '6991-ite-pair-BC',
    '6827-vivify-snapshot-proofs': '6990-ite-pair-proofs',
    '20260910-6824-vivify-snapshot-control/control-6824.json': '20260910-6969-ite-pair-hoist-build/control-6985.json',
    'Same-work Vivify snapshot omission on current6730': 'Same-work ITE outer-pair load hoist on current6730',
    'Avoid copying unused literal snapshots only in pure-irredundant non-flushing schedules.': 'Hoist immutable outer pair loads across inner comparisons; preserve all candidate/gate order, verification, ticks, budgets, proof and API.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
