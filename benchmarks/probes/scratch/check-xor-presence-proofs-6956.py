#!/usr/bin/env python3
"""Strong independent checks of actual old/new XOR-dependent certificates."""
from pathlib import Path
source = Path(__file__).with_name('check-factor-count-proofs-6932.py')
code = source.read_text()
for before, after in {
    '20260910-6932-factor-count-proofs':'20260910-6956-xor-presence-proofs',
    'test-factor-count-proof-6931.cpp':'test-xor-presence-proof-6955.cpp',
    '20260910-6925-factor-count-build':'20260910-6950-xor-presence-build',
    'control-6928.json':'control-6951.json',
    "'factor-count-6925'":"'xor-presence-6950'",
    '/factor/factor.cpp.o':'/congruence/congruence.cpp.o',
    '{arm}-factor-6928.o':'{arm}-congruence-6951.o',
    'product((3,9,33),(3,8),(1,2,4),(0,1),(0,1),(0,1))':
        'product((0,1,2),(0,1),(0,1),(1,2),(0,1))',
    '288':'96', '144':'48',
    '48 fixed native proofs per actual6730/6925 Factor arm; full grids and budget cuts,2 encounter orders,3 widths,3 row counts,2 column counts. Unlimited cases introduce a fresh variable before original input roots force a generated-quotient contradiction. Exact cross-arm input/certificate/state; no historical legacy-checker dependence.':
        '48 fixed native proofs per actual6730/6950 Congruence arm: two congruent XOR outputs, both parities, root-effective size3/4/5, duplicate copies and reversed encounter order. Original opposite output roots are assigned only after extraction; generated equivalence closes the proof. Exact cross-arm input/certificate/statistics; no historical legacy-checker dependence.',
    'Actual Factor object with hash-identical other6730 sources; independent full builds and real-instance B/C/runtime proofs still required. No source fuzz, broad competitiveness or promotion.':
        'Actual Congruence object with hash-identical other6730 sources; independent full builds and real-instance B/C/runtime proofs still required. No source fuzz, broad competitiveness or promotion.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
