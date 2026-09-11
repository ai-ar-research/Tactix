#!/usr/bin/env python3
"""288 exact-parent dense gate certificates with independent proof checkers."""
from pathlib import Path
base = Path(__file__).with_name('check-factor-arena-proofs-7181.py')
code = base.read_text()
changes = {
    '20260910-7181-factor-arena-proofs': '20260911-7198-congruence-fused-proofs',
    'test-factor-count-proof-6931.cpp': 'test-congruence-fused-proof-7198.cpp',
    '7180-factor-arena': '7196-congruence-fused',
    'runtime-factor-arena-7180': 'runtime-congruence-fused-7196',
    "'benchmarks/runs/20260910-' + stem": "'benchmarks/runs/' + ('20260911-' if arm == 'candidate' else '20260910-') + stem",
    'product((3, 9, 33), (3, 8), (1, 2, 4), (0, 1), (0, 1), (0, 1))':
        'product((2, 7, 65), (0, 1, 2), (0, 1), (0, 1), (0, 1), (0, 1))',
    'Actual retained7144 and arena7180 libraries, 144 fixed Factor proofs each; widths1/2/4, rows3/9/33, columns3/8, two orders, bounded/unbounded, DRAT/LRAT. Generated quotients cause the final contradiction. Exact input/certificate/state. Only owned fixed-case residue removed.':
        'Actual retained7144 and fused7196 libraries,144 dense AND closure proofs each; arities2/7/65, first/middle/last outputs, polarity, import order, public/direct dispatch and DRAT/LRAT. Exhaustive model-set preservation for arities2/7. Opposing equivalent gate outputs close the final proof. Exact input/certificate/state. Only owned fixed-case residue removed.',
    'PASS288 exact-parent Factor proofs': 'PASS288 exact-parent dense AND proofs',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
