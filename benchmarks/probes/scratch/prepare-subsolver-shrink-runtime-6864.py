#!/usr/bin/env python3
"""Three predeclared same-work waves after the independent SubSolver B/C gate."""
from pathlib import Path
source = Path(__file__).with_name('prepare-sweep-rootfree-runtime-6850.py')
code = source.read_text()
for before,after in {
    '6850-sweep-rootfree-runtime':'6864-subsolver-shrink-runtime',
    '6846-sweep-rootfree-BC':'6861-subsolver-shrink-BC',
    'summary-6848.json':'summary-6863.json',
    '6845-sweep-rootfree-proofs':'6860-subsolver-shrink-proofs',
    '6838-sweep-rootfree':'6857-subsolver-shrink',
    '20260910-6843-sweep-rootfree-control/control-6843.json':'20260910-6857-subsolver-shrink-build/control-6858.json',
    'tactix-sweep-rootfree-three-wave-runtime-v1':'tactix-subsolver-shrink-three-wave-runtime-v1',
    'Avoiding the intermediate copy for root-free Sweep environment clauses lowers implementation cost without altering work, budgets, scheduling or CDCL. Price total solver and Sweep timers; retain all adverse outcomes and low-slot control38. Independent of unadopted6822.':
        'Shrink-only SubSolver watch compaction reduces implementation cost without changing work, budgets, scheduling or CDCL. Price total solver, Sweep and BVE timers; retain every adverse outcome and low-slot control38. Independent of6838/6822; no layout change.',
    '113/113/112 full suites,300 actual old/new cases/1500 built environments with exact state/model/proof digests, existing full Sweep controls and120 independently checked fixed proofs. All33 B/C pairs exact.':
        '113/113/112 full suites,80 actual old/new configurations/1440 incremental solves with exact counter/model/LRAT/core digests; existing SubSolver/Sweep controls and120 independently checked fixed proofs. All33 B/C pairs exact.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
