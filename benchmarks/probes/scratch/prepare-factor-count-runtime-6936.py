#!/usr/bin/env python3
"""Predeclare Factor unit-cost and whole-solver tradeoff on32/39/38."""
from pathlib import Path
source = Path(__file__).with_name('prepare-vivify-snapshot-runtime-6831.py')
code = source.read_text()
for before, after in {
    '6831-vivify-snapshot-runtime':'6936-factor-count-runtime',
    '6828-vivify-snapshot-BC':'6933-factor-count-BC',
    'summary-6830.json':'summary-6935.json',
    '6827-vivify-snapshot-proofs':'6932-factor-count-proofs',
    "['checked'] == 48":"['checked'] == 288",
    '6822-vivify-snapshot':'6925-factor-count',
    '20260910-6824-vivify-snapshot-control/control-6824.json':'20260910-6925-factor-count-build/control-6928.json',
    'tactix-vivify-snapshot-three-wave-runtime-v1':'tactix-factor-count-three-wave-runtime-v1',
    'Omitting unused pure-irredundant non-flushing Vivify literal snapshots lowers implementation cost without altering work, budgets or CDCL. Price total solver and Vivify timers; retain all adverse outcomes and control38.':
        'Saving cofactor reference/value across touched-vector push removes reloads while preserving discovery publication before count update and exact work, proofs, budgets, API and CDCL. Price Factor timer and total/slot/outside/RSS with Sweep/BVE/Vivify/Substitute controls. Require a useful whole-binary tradeoff; no alignment or repeat-to-select tuning.',
    '113/113/112 full suites,432+216 actual old/new state/model/proof-digest cases,144 ordering cases and48 independent fixed proofs. All33 B/C pairs exact. Same immutable runtime runner and validated six-task-wave protocol;244 benchmark and19 watcher tests.':
        '113/113/112 full independent suites,270 actual old/new Factor controls with callback order and physical-state digest fd9ab4194fd8cb35, existingFactor/kernel suites,288 exact Factor-dependent DRAT/cake_lpr proofs and2 negative controls.6927 nonexistent optional test path classified6930 and corrected6928. All33 B/C pairs exact. Maintained262 bench/22 watcher tests; perchange test consideration retained.',
    "Path(__file__), root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/solver-summary-6817.json'":
        "Path(__file__), root/'benchmarks/runs/20260910-6925-factor-count-build/inspection-6930.json', root/'benchmarks/runs/20260910-6920-factor-cost-annotation/location-6921.json', root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/solver-summary-6817.json'",
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
