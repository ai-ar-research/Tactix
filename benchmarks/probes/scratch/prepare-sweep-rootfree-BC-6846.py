#!/usr/bin/env python3
"""Freeze independent B28+C5 Sweep same-work gate without changing older runs."""
from pathlib import Path
source = Path(__file__).with_name('prepare-vivify-snapshot-BC-6828.py')
code = source.read_text()
for before, after in {
    '6822-vivify-snapshot': '6838-sweep-rootfree',
    '6828-vivify-snapshot-BC': '6846-sweep-rootfree-BC',
    '6827-vivify-snapshot-proofs': '6845-sweep-rootfree-proofs',
    '20260910-6824-vivify-snapshot-control/control-6824.json': '20260910-6843-sweep-rootfree-control/control-6843.json',
    'Same-work Vivify snapshot omission on current6730': 'Same-work Sweep root-free temporary-copy omission on current6730',
    'Avoid copying unused literal snapshots only in pure-irredundant non-flushing schedules.': 'Avoid copying into temporary Sweep storage only when the clause has no root-false literals. Preserve filtering, environment order, proof IDs and all scheduling/CDCL behavior.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
