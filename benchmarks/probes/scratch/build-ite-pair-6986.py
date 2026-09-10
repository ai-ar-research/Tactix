#!/usr/bin/env python3
"""Independent full builds after exact actual-object ITE pairing controls."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
subprocess.run(['python3', str(root/'tools/slurm/session_deadline.py'), '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
assert (root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build/control-6985.json').exists()
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for before, after in {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-ite-pair-hoist-6969',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build': '20260910-6969-ite-pair-hoist-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6969-ite-pair-hoist-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}": "{'src/rules/congruence/congruence.cpp', 'tests/test_ite_pair.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.': 'Hoist immutable outer ITE conditional-pair loads across inner comparisons. No index/layout, ordering, verification, effort, guard, proof or scheduling change; independent6730 source, not composed with6950.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.': '560 fixed public ITE cases agree on actualold/new complete ordered gates/ticks, digest551691671817ff7b. All polarities, duplicate/reversed input, missing clauses, repeated extraction, small-model entailment and exactly256/over256 conditional lists covered; existing Congruence and1200 cleanup cases agree. Initial fixture guessed four instead of two entries per signed output and failed unchanged parent at65 gates; frozen6983/6984, corrected6985. Budget/stop controls, strong native proofs and B/C gates required before real-instance timing. Source fuzz unavailable; no promotion.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
