#!/usr/bin/env python3
"""Independent source suites for the behavior-preserving propagation experiment."""
from pathlib import Path
source=Path(__file__).with_name('build-substitute-retry-6233.py')
code=source.read_text()
for old,new in {
 '.claude/worktrees/runtime-substitute-retry-6231':'.claude/worktrees/runtime-shared-blocker-6448',
 '.claude/worktrees/runtime-learned-sort-6169':'.claude/worktrees/runtime-activation-stamp-6424',
 '20260909-6231-substitute-retry-build':'20260909-6448-shared-blocker-build',
 'build/pin-6231-substitute-retry-':'build/pin-6448-shared-blocker-',
 "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n":'',
 "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":"{'src/kernel/kernel.cpp', 'tests/test_shared_blocker.cpp'}",
 'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':'Share the satisfied-blocker exit after complete variable-width watch copying, as native propagation does. No rule, guard, proof, tick, candidate admission, memory layout or schedule change. Test generated code and exact parent state/work/proof before pricing the Rank32 hot path.',
 'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':'New180 fixed mixed-watch cases cover both entry widths/orders, same/compacted cursors, relocation/garbage, conflict tails, ignored large clauses, recording/persistent probing and warming continuation. Actual old/new object linkage must produce exact state digests, including existing224 propagation-cursor cases. Full109 Release twice108trace expected; broad BC and repeated timing required. This is unpromoted/unfuzzed investigation material.',
}.items():
    assert old in code,old; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
