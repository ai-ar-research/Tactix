#!/usr/bin/env python3
"""Build/test independent Release, BC and trace pins for stamp preservation."""
from pathlib import Path
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for before, after in (
    ('.claude/worktrees/runtime-substitute-retry-6231', '.claude/worktrees/runtime-compact-stamp-6357'),
    ('.claude/worktrees/runtime-learned-sort-6169', '.claude/worktrees/runtime-substitute-collapse-6305'),
    ('20260909-6231-substitute-retry-build', '20260909-6357-compact-stamp-build'),
    ('build/pin-6231-substitute-retry-', 'build/pin-6357-compact-stamp-'),
    ("    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n", ''),
    ("{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}", "{'src/policies/heuristics.cpp', 'tests/test_compact_stamp.cpp'}"),
    ('Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.',
     'Preserve detached queue records across physical compaction while restamping/remapping live members as before. Native finalize_compacting drops later fixed aliases before compact_queue: the first fixed representative retains its own stamp. No comparator, budget, guard, public action or proof change.'),
    ('Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.',
     '24 fixed fixtures cover both import orders, both ranking modes, identity/dead-variable remaps, three stamps including overflow boundary, repeated compaction, queue/cursor invariants and public literal identities/surface. Frozen-parent differential and actual native compaction control required. Full three-pin suites; B/C gate and Rank32 causal replay follow.'),
):
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
