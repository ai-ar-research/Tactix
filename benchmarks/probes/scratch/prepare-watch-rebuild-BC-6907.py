#!/usr/bin/env python3
"""Freeze unchanged B28+C5 comparison for exact binary-only scan omission."""
from pathlib import Path
source = Path(__file__).with_name('prepare-subsolver-shrink-BC-6861.py')
code = source.read_text()
for before,after in {
    '6857-subsolver-shrink':'6902-watch-rebuild',
    '6861-subsolver-shrink-BC':'6907-watch-rebuild-BC',
    '6860-subsolver-shrink-proofs':'6910-watch-rebuild-proofs',
    'control-6858.json':'control-6904.json',
    'Same-work SubSolver watch-list shrink-only compaction on current6730':'Same-work binary-only watch-rebuild scan omission on current6730',
    'Express compaction as shrink-only erasure instead of generic resize; preserve watch contents/capacity/order and all proof, effort, scheduling and CDCL behavior.':
        'Skip scanning watch words when exact per-list binary count proves no large entries, retaining old order-bit normalization and mixed-list compaction. Preserve all state, proofs, scheduling, effort and CDCL.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
