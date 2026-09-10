#!/usr/bin/env python3
"""Build/test three independent pins and freeze the layout-only patch."""
from pathlib import Path
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
changes = {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-watermark-layout-6763',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build': '20260909-6763-watermark-layout-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6763-watermark-layout-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}": "{'src/kernel/kernel.hpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Place the unchanged historical Reduce watermark in existing flag-to-pointer padding, restoring watched-search member offsets. No semantic, guard, public-surface, proof or scheduling change. Matched cost ablation, not assumed optimization.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        'All288 existing watermark states and complete independent Release/BC/trace suites required. Separate fixed compiler-layout regression compares every measured parent6654 offset and proves marker fits unused padding. No new semantic case needs a new production test; layout-specific oracle remains a fixed diagnostic. Exact work/proofs and B/C plus repeated Rank38/46 runtime required; no fuzz/promotion authority.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
