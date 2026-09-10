#!/usr/bin/env python3
"""Freeze unchanged B28+C5 for existing-byte XOR membership reuse."""
from pathlib import Path
import json
root = Path(__file__).resolve().parents[3]
proof = json.loads((root/'benchmarks/runs/20260910-6956-xor-presence-proofs/summary.json').read_text())
assert proof['checked'] == 96 and proof['negative_controls'] == 2
assert (root/'benchmarks/runs/20260910-6950-xor-presence-build/index-control-6953.json').exists()
source = Path(__file__).with_name('prepare-vivify-snapshot-BC-6828.py')
code = source.read_text()
for before, after in {
    '6822-vivify-snapshot':'6950-xor-presence',
    '6828-vivify-snapshot-BC':'6957-xor-presence-BC',
    '6827-vivify-snapshot-proofs':'6956-xor-presence-proofs',
    '20260910-6824-vivify-snapshot-control/control-6824.json':'20260910-6950-xor-presence-build/control-6951.json',
    'Same-work Vivify snapshot omission on current6730':'Same-work XOR membership reuse on current6730',
    'Avoid copying unused literal snapshots only in pure-irredundant non-flushing schedules.':
        'Reuse existing ternary-presence bytes for parity completeness, preserving exact order, ticks, root-effective indexing, incomplete-index behavior, proof and API.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
