#!/usr/bin/env python3
"""Three exact-work waves, total/Sweep/BVE cost and12-byte host-memory risk."""
from pathlib import Path
source = Path(__file__).with_name('prepare-subsolver-shrink-runtime-6864.py')
code = source.read_text()
for before,after in {
    '6864-subsolver-shrink-runtime':'6882-subsolver-binary-runtime',
    '6861-subsolver-shrink-BC':'6873-subsolver-binary-BC',
    'summary-6863.json':'summary-6877.json',
    '6860-subsolver-shrink-proofs':'6872-subsolver-binary-proofs',
    '6857-subsolver-shrink':'6869-subsolver-binary',
    'control-6858.json':'control-6874.json',
    'tactix-subsolver-shrink-three-wave-runtime-v1':'tactix-subsolver-binary-three-wave-runtime-v1',
    'Shrink-only SubSolver watch compaction reduces implementation cost without changing work, budgets, scheduling or CDCL. Price total solver, Sweep and BVE timers; retain every adverse outcome and low-slot control38. Independent of6838/6822; no layout change.':
        'Approved full-width12-byte binary-tagged SubSolver watches reduce Kitten binary arena loads without changing work, reference8-byte effort, scheduling or CDCL. Price total solver, Sweep/BVE timers and peak RSS; retain all adverse outcomes and low-slot control38. Independent6730 branch, no composition.50% per-watch storage growth is a risk, not an accepted tradeoff.',
    '113/113/112 full suites,80 actual old/new configurations/1440 incremental solves with exact counter/model/LRAT/core digests; existing SubSolver/Sweep controls and120 independently checked fixed proofs. All33 B/C pairs exact.':
        '113/113/112 complete own-source suites.6874 own-arm header/object controls match720 configurations/12960 solves;6881 direct core rebuild/shuffle matches80 configurations/320 solves.120 external fixed proofs pass. Mixed-header6870 not acceptance;6879 fixture compile failure preserved. All33 B/C pairs exact. Current-source fuzz and full50/memory acceptance remain open.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
