#!/usr/bin/env python3
"""Localize Rank47 gate extraction after exact own-pin solver replay."""
from pathlib import Path
source = Path(__file__).with_name('annotate-subsolver-6856.py')
code = source.read_text()
for before, after in {
    '20260910-6815-inprocessing-release-profiles':'20260910-6945-congruence-release-profiles',
    'solver-summary-6817.json':'solver-summary-6947.json',
    "s['arms']['39/candidate']":"s['arms']['47/candidate']",
    'profiles/002.perf.data.xz':'profiles/000.perf.data.xz',
    'src/engine/subsolver.cpp':'src/rules/congruence/congruence.cpp',
    '20260910-6856-subsolver-propagation-annotation':'20260910-6948-congruence-extraction-annotation',
    'tactix::SubSolver::propagate()':'tactix::extract_gates(tactix::Kernel const&, std::vector<tactix::CongGate, std::allocator<tactix::CongGate> >&, bool, tactix::Kernel*)',
    '0x4e8c80':'0x45eaa0',
    'Existing checked current6730 Rank39 samples only.':
        'Rank47 current6730 exact unprofiled solver-work/certificate replay; independent proof checks pending at6947 snapshot. Extraction5.53% program self samples. Inspect XOR membership and existing ternary-presence reuse as a possible source-only same-work cost reduction; do not change record/merge order or redesign the index.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
