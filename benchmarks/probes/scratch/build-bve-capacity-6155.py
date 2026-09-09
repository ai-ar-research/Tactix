#!/usr/bin/env python3
"""Build the finer callback observer with an actual old-object regression."""
from pathlib import Path

source = Path(__file__).with_name('build-collect-memory-6147.py')
code = source.read_text()
changes = {
    'runtime-collect-memory-6146': 'runtime-bve-capacity-6154',
    '20260908-6146-collect-memory-build': '20260909-6154-bve-capacity-build',
    'build/pin-6127-publication-trace': 'build/pin-6151-collect-memory-trace',
    'tests/test_collect_memory.cpp': 'tests/test_bve_memory.cpp',
    'row == capacities.size() * stages.size()': 'rows.size() == 3',
    "f'build/pin-{number}-collect-memory-{kind}'": "f'build/pin-{number}-bve-capacity-{kind}'",
    "pool.submit(build, 6146, 'release'": "pool.submit(build, 6154, 'release'",
    "pool.submit(build, 6147, 'trace'": "pool.submit(build, 6155, 'trace'",
    'build/pin-6146-collect-memory-release': 'build/pin-6154-bve-capacity-release',
    "parent='.claude/worktrees/runtime-substitute-journal-6136'": "parent='.claude/worktrees/runtime-collect-memory-6146'",
    'Locate BVE peak before collector, during fresh/old arena overlap, or dense rebuilding; do not infer ownership from slot-end retained capacity.':
    'Checked rank9 first peak precedes GC; identify capacity-growing BVE candidate intervals and distinguish clause,reconstruction,watch anddense arenas without per-candidate output.',
    'Old actual trace objects lack required24 checkpoint rows. New fixed sparse,dense,empty collection cases verify8 stages, exact scratch ownership before/after swap, final arena identity and dense refs. Complete Release/trace suites; byte-identical Release is required before inheriting6107 gates.':
    'Old actual trace objects fail required3 capacity-transition rows. New fixture requires exact begin/end subject labels and suppresses unchanged callbacks; collector fixture remains. Complete Release/trace suites and exact Release identity required.',
    'Diagnostic instrumentation only; no production behavior change or memory/runtime improvement claim. Collector-local radix scratch and within-candidate allocations need finer sampling if the boundary evidence does not discriminate.':
    'Diagnostic only,no runtime ormemory gain. Capacity transitions bracket intervals,not single allocations; temporary candidate buffers may grow andfree without a sampled pool changing. Full-work/proof and old-slot/collector capacity replay required.',
}
for old, new in changes.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
