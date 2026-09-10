#!/usr/bin/env python3
"""Build every consumer of the approved enlarged watch layout independently."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
subprocess.run(['python3', str(root/'tools/slurm/session_deadline.py'), '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for before, after in {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-subsolver-binary-6869',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build': '20260910-6869-subsolver-binary-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6869-subsolver-binary-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/engine/subsolver.cpp', 'src/engine/subsolver.hpp', 'tests/test_subsolver_binary.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'User-approved full-width12-byte binary-tagged SubSolver watch prototype. Kitten-mode binaries skip arena loads, preserving reference8-byte effort currency, proofs, ordering, capacities and fixed-mode behavior. Independent6730 branch, no composition or promotion.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        'Initial control6870 passed but mixed parent-header consumer objects with the changed watch layout; preserve as non-acceptance diagnostic only. Full independent builds of all consumers here, then fresh own-arm object controls6874 and external fixed proofs required before B/C. Added720 configurations/12960 incremental solves including native cache-line boundaries and model flips. Runtime, memory, current-source fuzz and full50 gates remain separate.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
