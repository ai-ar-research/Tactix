#!/usr/bin/env python3
"""Predeclare ITE target47, Congruence transfer42 and low-slot control38."""
from pathlib import Path
source = Path(__file__).with_name('prepare-vivify-snapshot-runtime-6831.py')
code = source.read_text()
for before, after in {
    '6831-vivify-snapshot-runtime': '6994-ite-pair-runtime',
    '6828-vivify-snapshot-BC': '6991-ite-pair-BC',
    'summary-6830.json': 'summary-6993.json',
    '6827-vivify-snapshot-proofs': '6990-ite-pair-proofs',
    "['checked'] == 48": "['checked'] == 384",
    '6822-vivify-snapshot': '6969-ite-pair-hoist',
    '(32, 39, 38)': '(47, 42, 38)',
    '20260910-6824-vivify-snapshot-control/control-6824.json': '20260910-6969-ite-pair-hoist-build/control-6985.json',
    'tactix-vivify-snapshot-three-wave-runtime-v1': 'tactix-ite-pair-three-wave-runtime-v1',
    "{'32':'broad inprocessing exact-native-conflict/decision control', '39':'Sweep-heavy inprocessing transfer control; native work differs', '38':'known code-cost-sensitive low-slot adverse control'}": "{'47':'profiled ITE extraction hot loop; native internal work differs', '42':'Congruence-heavy transfer control; extra native-relative closures remain open', '38':'known code-cost-sensitive low-slot adverse control'}",
    'Omitting unused pure-irredundant non-flushing Vivify literal snapshots lowers implementation cost without altering work, budgets or CDCL. Price total solver and Vivify timers; retain all adverse outcomes and control38.': 'Hold immutable outer ITE conditional pair across inner comparisons and output growth. Actual assembly removes repeated outer loads; no index/layout/policy change. Price whole solver, Congruence, slot/outside and RSS against6730 across all three47/42/38 waves. Retain adverse outcomes; no repeat-to-select or unpriced composition with6950.',
    '113/113/112 full suites,432+216 actual old/new state/model/proof-digest cases,144 ordering cases and48 independent fixed proofs. All33 B/C pairs exact. Same immutable runtime runner and validated six-task-wave protocol;244 benchmark and19 watcher tests.': 'Independent113/113/112 full suites and matching release pins;560 actual ordered ITE extraction cases551691671817ff7b,448 actual budget/stop/index cases3b02f64fd29a9493 and existingCongruence/1200cleanup cases.384 ITE-dependent cake_lpr/DRAT proofs and2 negatives; all33 B/C pairs exact. Existing runtime/scoring/wave tests apply unchanged. No source fuzz or promotion.',
    "Path(__file__), root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/solver-summary-6817.json'": "Path(__file__), root/'benchmarks/runs/20260910-6945-congruence-release-profiles/solver-summary-6947.json', root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build/budget-control-6988.json', root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build/compile-6970.json'",
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
