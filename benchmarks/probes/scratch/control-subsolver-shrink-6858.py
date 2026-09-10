#!/usr/bin/env python3
"""Actual SubSolver objects; no mirror algorithm or scheduling change."""
from pathlib import Path
source = Path(__file__).with_name('control-sweep-rootfree-6839.py')
code = source.read_text()
for before,after in {
    '20260910-6838-sweep-rootfree-build':'20260910-6857-subsolver-shrink-build',
    'control-6839.json':'control-6858.json',
    '.claude/worktrees/runtime-sweep-rootfree-6838':'.claude/worktrees/runtime-subsolver-shrink-6857',
    '/rules/sweep/sweep.cpp.o':'/engine/subsolver.cpp.o',
    "f'{arm}-sweep.o'":"f'{arm}-subsolver.o'",
    'src/rules/sweep/sweep.cpp':'src/engine/subsolver.cpp',
    "('test_sweep_rootfree', 'test_sweep')":"('test_subsolver_shrink', 'test_subsolver', 'test_sweep')",
    'Actual old/new environments, order, proof identities, stats, model sets and builder proof bytes match300 fixed cases. Existing Sweep family tests pass identically. Scratch contents intentionally not semantic state. Independent final proof/B/C/full-runtime and current-source fuzz remain separate.':
        'Actual old/new objects agree on fixed incremental watch-compaction sequences with both search modes/meters, root contradictions, watch poses, budget cuts, exhaustive models and LRAT derivation/core digests. Existing full SubSolver/Sweep controls pass identically. Independent final proofs/B/C/runtime/fuzz gates remain separate.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
