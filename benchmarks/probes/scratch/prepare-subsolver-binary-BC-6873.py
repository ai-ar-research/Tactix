#!/usr/bin/env python3
"""Independent approved-layout B28+C5 gate after complete own-arm controls."""
from pathlib import Path
source = Path(__file__).with_name('prepare-sweep-rootfree-BC-6846.py')
code = source.read_text()
for before,after in {
    '6838-sweep-rootfree':'6869-subsolver-binary',
    '6846-sweep-rootfree-BC':'6873-subsolver-binary-BC',
    '6845-sweep-rootfree-proofs':'6872-subsolver-binary-proofs',
    '20260910-6843-sweep-rootfree-control/control-6843.json':'20260910-6869-subsolver-binary-build/control-6874.json',
    'Same-work Sweep root-free temporary-copy omission on current6730':'Approved full-width binary-tagged SubSolver watches against current6730',
    'Avoid copying into temporary Sweep storage only when the clause has no root-false literals. Preserve filtering, environment order, proof IDs and all scheduling/CDCL behavior.':
        'Skip clause-arena loads for Kitten-mode binary watches with a12-byte full-width tagged representation. Preserve reference8-byte effort, proof identities, work and scheduling/CDCL behavior; memory increase is a measured risk, not accepted parity.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
