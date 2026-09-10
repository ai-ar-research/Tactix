#!/usr/bin/env python3
"""Localize the existing checked Rank32 private propagation cost; no solver."""
from pathlib import Path
source = Path(__file__).with_name('annotate-subsolver-6856.py')
code = source.read_text()
for before,after in {
    "s['arms']['39/candidate']":"s['arms']['32/candidate']",
    'profiles/002.perf.data.xz':'profiles/000.perf.data.xz',
    'src/engine/subsolver.cpp':'src/kernel/kernel.cpp',
    '20260910-6856-subsolver-propagation-annotation':'20260910-6888-vivify-propagation-annotation',
    'tactix::SubSolver::propagate()':'bool tactix::propagate_literal<false, false, true>(tactix::Kernel&, unsigned int, unsigned int, unsigned long&)',
    '0x4e8c80':'0x412810',
    'Existing checked current6730 Rank39 samples only.':'Existing checked current6730 Rank32 samples only, private propagation under Vivify. No source change or speedup claim.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
