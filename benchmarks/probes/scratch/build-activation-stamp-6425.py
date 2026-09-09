#!/usr/bin/env python3
"""Full independent Release/BC/trace builds for activation stamp repair."""
from pathlib import Path
source=Path(__file__).with_name('build-substitute-retry-6233.py')
code=source.read_text()
for old,new in {
 '.claude/worktrees/runtime-substitute-retry-6231':'.claude/worktrees/runtime-activation-stamp-6424',
 '.claude/worktrees/runtime-learned-sort-6169':'.claude/worktrees/runtime-vivify-count-cache-6383',
 '20260909-6231-substitute-retry-build':'20260909-6424-activation-stamp-build',
 'build/pin-6231-substitute-retry-':'build/pin-6424-activation-stamp-',
 "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n":'',
 "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":"{'src/policies/heuristics.cpp', 'tests/test_activation_stamp.cpp'}",
 'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':'Seed queue numeric stamps by first activation, matching native enqueue, instead of retaining allocation-plus-bump offsets. Preserve existing queue order, score seeding, public action surface, proofs and compaction retention; no benchmark constants or policy selector.',
 'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':'Actual native6423 parser/compaction exposes16 fixed-key mismatches in6357. New tests cover both attachment paths, header-only identities, import direction, root signs, repeated compaction, inactive numeric keys, score/queue cursor consistency and public Decide surface. No calculus/guard change; broadBC and causal replay required, source fuzz unavailable, no promotion.',
}.items():
    assert old in code,old; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
