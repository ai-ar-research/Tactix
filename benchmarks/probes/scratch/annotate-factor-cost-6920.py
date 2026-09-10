#!/usr/bin/env python3
"""Inspect existing checked Rank32 Factor samples; no source or solver run."""
from pathlib import Path
source = Path(__file__).with_name('annotate-subsolver-6856.py')
code = source.read_text()
for before,after in {
    "s['arms']['39/candidate']":"s['arms']['32/candidate']",
    'profiles/002.perf.data.xz':'profiles/000.perf.data.xz',
    'src/engine/subsolver.cpp':'src/rules/factor/factor.cpp',
    '20260910-6856-subsolver-propagation-annotation':'20260910-6920-factor-cost-annotation',
    'tactix::SubSolver::propagate()':'tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)',
    '0x4e8c80':'0x47e130',
    'Existing checked current6730 Rank39 samples only.':
        'Existing checked current6730 Rank32 Factor samples only. Native/current86 calls,7357 accepted factorizations and428217051 logical ticks agree; timer clocks differ and no precise native unit-price claim follows. Existing intern table and lazy remainder cache are already optimized; identify a repeated cost before changing them.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
