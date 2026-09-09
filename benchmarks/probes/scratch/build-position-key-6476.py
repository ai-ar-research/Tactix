#!/usr/bin/env python3
"""Independent suites for temporary radix-key width reduction."""
from pathlib import Path
source=Path(__file__).with_name('build-substitute-retry-6233.py')
code=source.read_text()
for old,new in {
 '.claude/worktrees/runtime-substitute-retry-6231':'.claude/worktrees/runtime-position-key-6475',
 '.claude/worktrees/runtime-learned-sort-6169':'.claude/worktrees/runtime-activation-stamp-6424',
 '20260909-6231-substitute-retry-build':'20260909-6475-position-key-build',
 'build/pin-6231-substitute-retry-':'build/pin-6475-position-key-',
 "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n":'',
 "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":"{'src/kernel/kernel.cpp', 'tests/test_position_keys.cpp'}",
 'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':'Narrow only temporary logical-position radix keys from64 to32bits: the existing four passes consume exactly those bits. Full-width early-order check, stable order, alias storage, proof ids, public APIs, scheduling and memory layout remain unchanged. Two key buffers halve payload; total internal sort allocation falls20n to12n bytes. No fullsolver RSS/runtime claim.',
 'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':'New480 fixed order/allocation fixtures span empty/single/multiple input sizes, zero/high proof ids, alias-free and ordered bypasses, ties, each byte boundary, full32bit positions and artificial wider aliases preserving existing low-word behavior. Independent stable comparison-sort oracle, exact arena/alias preservation and actual allocator counts. Actual old/new object controls plus109Release twice108trace andBC required. Isolated/unfuzzed; not promotion.',
}.items():
    assert old in code,old; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
