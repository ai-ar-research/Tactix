#!/usr/bin/env python3
"""Build independent pins only after actual old/new fixed controls agree."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
subprocess.run(['python3', str(root/'tools/slurm/session_deadline.py'), '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
assert (root/'benchmarks/runs/20260910-6843-sweep-rootfree-control/control-6843.json').exists()
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for before, after in {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-sweep-rootfree-6838',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build': '20260910-6838-sweep-rootfree-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6838-sweep-rootfree-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/rules/sweep/sweep.cpp', 'tests/test_sweep_rootfree.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Avoid the temporary literal copy when a Sweep environment clause has no root-false literals. Original main-arena/stack-binary inputs stay stable during env_add. Preserve all environment ordering, dedup and filtering charges, proof IDs, public actions, scheduling budgets and CDCL. Independent from6822; do not compose unpriced experiments.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        '6843 actual old/new Sweep objects agree on300 cases/1500 built environments, exhaustive model sets, encoded state/order/stats and3080 proof bytes, digest5c9480aed6b712d1. Existing full Sweep family suite agrees, including86501 environments/5344 claims. Fixture-only compile and contradictory-input failures frozen6840/6842 before candidate execution. Full independent suites, fixed independent proofs, B28+C5 and three exact-work timing waves required. Unfuzzed/unpromoted; no gain claimed.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
