#!/usr/bin/env python3
"""Parent-negative fixed matrix, then independent Release/BC/trace pins."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260909-6730-reduce-watermark-build'
parent = root / 'build/pin-6654-congruence-empty-gates-release'
test = root / '.claude/worktrees/runtime-reduce-watermark-6730/tests/test_reduce_watermark.cpp'
output = c / 'test-parent-watermark'
assert not output.exists()
command = ['c++', '-std=c++20', '-O2', '-fno-exceptions', '-fno-rtti',
           '-I' + str(root / '.claude/worktrees/runtime-congruence-empty-gates-6654/src'),
           '-I' + str(root / '.claude/worktrees/runtime-congruence-empty-gates-6654/include'),
           str(test), *map(str, sorted((parent / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))), '-o', str(output)]
for name, args in [('parent-compile', command), ('parent-negative', [str(output), '--expect-parent'])]:
    with (c / 'logs' / (name + '.log')).open('x') as out:
        result = subprocess.run(args, cwd=c, stdout=out, stderr=subprocess.STDOUT)
    assert result.returncode == 0, name
    print(name, 'PASS', flush=True)
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
changes = {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-reduce-watermark-6730',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-congruence-empty-gates-6654',
    '20260909-6231-substitute-retry-build': '20260909-6730-reduce-watermark-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6730-reduce-watermark-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/kernel/kernel.cpp', 'src/kernel/kernel.hpp', 'src/rules/reduce/reduce.cpp', 'tests/test_reduce_watermark.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Preserve historical Reduce watermark across promotion; capture collection start before advancing next-call metadata. Genuine collection resets; implementation-only packing forwards. No guard, instance, proof primitive or scheduling admission change.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        '288 fixed old/new states cover promotion, retired boundary, irredundant control, real versus implementation-only packing, aliases and id zero, root/nonroot, zero-victim and compact reductions, pure public enumeration and repeated collection. Parent exposes32 expected cursor failures. Native/certificate/B/C/Rank46 causal and runtime gates follow. No fuzz or promotion authority.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
