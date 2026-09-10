#!/usr/bin/env python3
"""Three predeclared same-work waves after the complete B/C solver gate."""
from pathlib import Path
source = Path(__file__).with_name('prepare-vivify-snapshot-runtime-6831.py')
code = source.read_text()
for before,after in {
    '6831-vivify-snapshot-runtime':'6912-watch-rebuild-runtime',
    '6828-vivify-snapshot-BC':'6907-watch-rebuild-BC',
    'summary-6830.json':'summary-6909.json',
    '6827-vivify-snapshot-proofs':'6910-watch-rebuild-proofs',
    "['checked'] == 48":"['checked'] == 128",
    '6822-vivify-snapshot':'6902-watch-rebuild',
    '20260910-6824-vivify-snapshot-control/control-6824.json':'20260910-6902-watch-rebuild-build/control-6904.json',
    'tactix-vivify-snapshot-three-wave-runtime-v1':'tactix-watch-rebuild-three-wave-runtime-v1',
    'Omitting unused pure-irredundant non-flushing Vivify literal snapshots lowers implementation cost without altering work, budgets or CDCL. Price total solver and Vivify timers; retain all adverse outcomes and control38.':
        'Omit binary-only reconnect word scans while preserving exact order-bit normalization, mixed-list compaction and complete state/proof/scheduling/CDCL behavior. Price total solver, slot, Vivify/Substitute/Sweep/BVE timers and RSS on all32/39/38 controls; no new representation or tuning.',
    '113/113/112 full suites,432+216 actual old/new state/model/proof-digest cases,144 ordering cases and48 independent fixed proofs. All33 B/C pairs exact. Same immutable runtime runner and validated six-task-wave protocol;244 benchmark and19 watcher tests.':
        '113/113/112 independent full suites,144 exact actual-kernel physical-image cases perarm, full kernel and840 replacement-cursor controls.128 exact reconnection-dependent fixed proofs verified by DRAT-trim/cake_lpr plus2 negative controls; legacy lrat-check is excluded after falseacceptance6906/classification6911. All33 B/C pairs exact. Same immutable runtime runner and validated threewave protocol; new8 strict proof-status tests.',
    "Path(__file__), root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/solver-summary-6817.json'":
        "Path(__file__), root/'benchmarks/runs/20260910-6902-watch-rebuild-build/scope-6911.json', root/'benchmarks/runs/20260910-6900-watch-rebuild-annotation/location-6901.json', root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/solver-summary-6817.json'",
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
