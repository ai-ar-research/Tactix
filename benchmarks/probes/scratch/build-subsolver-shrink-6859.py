#!/usr/bin/env python3
"""Build independent full pins after exact old/new SubSolver controls."""
from pathlib import Path
source = Path(__file__).with_name('build-sweep-rootfree-6844.py')
code = source.read_text()
for before,after in {
    '20260910-6843-sweep-rootfree-control/control-6843.json':'20260910-6857-subsolver-shrink-build/control-6858.json',
    'runtime-sweep-rootfree-6838':'runtime-subsolver-shrink-6857',
    '6838-sweep-rootfree':'6857-subsolver-shrink',
    "{'src/rules/sweep/sweep.cpp', 'tests/test_sweep_rootfree.cpp'}":"{'src/engine/subsolver.cpp', 'tests/test_subsolver_shrink.cpp'}",
    'Avoid the temporary literal copy when a Sweep environment clause has no root-false literals. Original main-arena/stack-binary inputs stay stable during env_add. Preserve all environment ordering, dedup and filtering charges, proof IDs, public actions, scheduling budgets and CDCL. Independent from6822; do not compose unpriced experiments.':
        'Express the two SubSolver propagation watch-list compaction exits as shrink-only erase ranges, not generic resize. Same element order, capacity, blockers, references, ticks, proofs and both search modes. Native Kitten assigns the compacted end directly. No layout, kernel, budget or scheduler change; independent of6838/6822.',
    '6843 actual old/new Sweep objects agree on300 cases/1500 built environments, exhaustive model sets, encoded state/order/stats and3080 proof bytes, digest5c9480aed6b712d1. Existing full Sweep family suite agrees, including86501 environments/5344 claims. Fixture-only compile and contradictory-input failures frozen6840/6842 before candidate execution. Full independent suites, fixed independent proofs, B28+C5 and three exact-work timing waves required. Unfuzzed/unpromoted; no gain claimed.':
        '6858 actual old/new SubSolver objects agree on80 configurations/1440 incremental solves:200SAT,627UNSAT,613Budget, digesteaeeec26246b290c. Both search modes/meters, poses, root contradictions, repeated budget cuts, exhaustive models and LRAT/core checks. Existing full SubSolver/Sweep suites agree. Full independent suites, fixed external proofs, B28+C5 and three exact-work timing waves required. Unfuzzed/unpromoted; no gain claimed.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
