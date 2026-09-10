#!/usr/bin/env python3
"""Build independent Release/BC/trace pins after actual old/new replay passes."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
subprocess.run(['python3', str(root/'tools/slurm/session_deadline.py'), '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
assert (root/'benchmarks/runs/20260910-6824-vivify-snapshot-control/control-6824.json').exists()
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for before, after in {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-vivify-snapshot-6822',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build': '20260910-6822-vivify-snapshot-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6822-vivify-snapshot-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/rules/vivify/vivify.cpp', 'tests/test_vivify_snapshot.cpp', 'tests/test_vivify_count_cache.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Omit full literal snapshots only for pure-irredundant non-flushing Vivify schedules. Read the final maximum count directly from each surviving reference after the two unchanged root-flush passes. Preserve mixed/public scopes, prefix flushing, pose, ordering, work ticks, budgets, proofs and CDCL. Native countref uses one maximum count; no new heuristic or kernel primitive.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        '6824 actual old/new objects match432 baseline plus216 new population/rewrite/scratch/state/model/proof cases and144 ordering cases; candidateABI40B/align8 unchanged. The new helper initially renamed main without explicit return; failed parent fixture6823 diagnosed and preserved6825, corrected with return0 before successful6824. No solver failure or fuzz evidence. Full independent suites,48 checked fixed proofs,B28+C5,and three paired exact-work timing waves required; isolated/unfuzzed,no promotion.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
