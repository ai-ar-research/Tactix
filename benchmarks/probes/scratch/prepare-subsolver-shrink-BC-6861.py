#!/usr/bin/env python3
"""Pin the existing B28+C5 gate for the independent watch-shrink experiment."""
from pathlib import Path
source = Path(__file__).with_name('prepare-sweep-rootfree-BC-6846.py')
code = source.read_text()
for before,after in {
    '6838-sweep-rootfree':'6857-subsolver-shrink',
    '6846-sweep-rootfree-BC':'6861-subsolver-shrink-BC',
    '6845-sweep-rootfree-proofs':'6860-subsolver-shrink-proofs',
    '20260910-6843-sweep-rootfree-control/control-6843.json':'20260910-6857-subsolver-shrink-build/control-6858.json',
    'Same-work Sweep root-free temporary-copy omission on current6730':'Same-work SubSolver watch-list shrink-only compaction on current6730',
    'Avoid copying into temporary Sweep storage only when the clause has no root-false literals. Preserve filtering, environment order, proof IDs and all scheduling/CDCL behavior.':
        'Express compaction as shrink-only erasure instead of generic resize; preserve watch contents/capacity/order and all proof, effort, scheduling and CDCL behavior.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
