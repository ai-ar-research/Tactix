#!/usr/bin/env python3
"""Localize checked Rank32 large-watch reconnect cost without a solver run."""
from pathlib import Path
source = Path(__file__).with_name('annotate-subsolver-6856.py')
code = source.read_text()
for before,after in {
    "s['arms']['39/candidate']":"s['arms']['32/candidate']",
    'profiles/002.perf.data.xz':'profiles/000.perf.data.xz',
    'src/engine/subsolver.cpp':'src/kernel/kernel.cpp',
    '20260910-6856-subsolver-propagation-annotation':'20260910-6900-watch-rebuild-annotation',
    'tactix::SubSolver::propagate()':'tactix::kernel_rebuild_large_watches(tactix::Kernel&, bool)',
    '0x4e8c80':'0x410c60',
    'Existing checked current6730 Rank39 samples only.':'Existing checked current6730 Rank32 large-watch rebuilding samples only. No new solver/source or speedup claim.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
