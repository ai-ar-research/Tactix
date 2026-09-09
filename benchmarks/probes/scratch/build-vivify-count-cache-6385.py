#!/usr/bin/env python3
"""Build and fully test independent immutable Release, BC and trace pins."""
from pathlib import Path
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for old, new in {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-vivify-count-cache-6383',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-compact-stamp-6357',
    '20260909-6231-substitute-retry-build': '20260909-6383-vivify-count-cache-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6383-vivify-count-cache-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}": "{'src/rules/vivify/vivify.cpp', 'tests/test_vivify_count_cache.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.': 'Cache each final Vivify schedule key leading occurrence count in existing target-ABI record padding. Preserve all comparisons, scope, priority, pose, prefix flush, proof, N1 and kernel contracts. Native countref caches its first count; CaDiCaL occurrence ordering remains unchanged.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.': '6384 actual old/new controls match432 repeated canonical/order/model/proof-digest fixtures and144 population/scope ordering fixtures. Both real source layouts40B/align8. New CTest covers every scope, reuse, flush, priorities, root-false, proof mode, cut/repeated rounds and empty scratch. No semantic calculus or guard change; retain owning C3 documentation. Full suites and independent proofs/BC/exact-work runtime gates follow; Claude-owned source fuzz unavailable, no promotion.',
}.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
