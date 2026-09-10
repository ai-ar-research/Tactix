#!/usr/bin/env python3
"""Recheck fixed collector proofs and require exact predecessor certificates."""
from pathlib import Path
source = Path(__file__).with_name('check-reduce-watermark-proofs-6734.py')
code = source.read_text()
changes = {
    '20260909-6734-reduce-watermark-proof-control': '20260909-6769-watermark-layout-proof-control',
    'build/pin-6730-reduce-watermark-release': 'build/pin-6763-watermark-layout-release',
    '20260909-6730-reduce-watermark-build': '20260909-6763-watermark-layout-build',
    'assert len(records)==8':
        "assert len(records)==8\nprior=root/'benchmarks/runs/20260909-6734-reduce-watermark-proof-control/fixed-proofs'\nassert {p.name for p in proofs.iterdir()} == {p.name for p in prior.iterdir()}\nfor p in proofs.iterdir(): assert sha(p)==sha(prior/p.name),p\nprint('All fixed input/proof bytes exact6730')",
    'Inherited fixed collector controls on current6730:': 'Inherited fixed collector controls on layout6763, all input/proof bytes exact6730:'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
