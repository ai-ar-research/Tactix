#!/usr/bin/env python3
"""Parent-negative retry matrix and three independent immutable build suites."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260910-7030-collect-retry-build'
parent = root / 'build/pin-6730-reduce-watermark-release'
test = root / '.claude/worktrees/runtime-collect-retry-7030/tests/test_collect_retry.cpp'
output = c / 'test-parent-collect-retry'
assert not output.exists()
command = ['c++', '-std=c++20', '-O2', '-fno-exceptions', '-fno-rtti',
    '-I' + str(root / '.claude/worktrees/runtime-reduce-watermark-6730/src'),
    '-I' + str(root / '.claude/worktrees/runtime-reduce-watermark-6730/include'),
    str(test), *map(str, sorted((parent / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))),
    '-o', str(output)]
for name, args in [('parent-compile', command), ('parent-negative', [str(output), '--expect-parent'])]:
    with (c / 'logs' / (name + '.log')).open('x') as out:
        result = subprocess.run(args, cwd=c, stdout=out, stderr=subprocess.STDOUT)
    assert result.returncode == 0, name
    print(name, 'PASS', flush=True)
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
changes = {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-collect-retry-7030',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build': '20260910-7030-collect-retry-build',
    'build/pin-6231-substitute-retry-': 'build/pin-7030-collect-retry-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/kernel/kernel.cpp', 'src/kernel/kernel.hpp', 'tests/test_collect_retry.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Suppress satisfied-large retirement advice only in sparse root collection, following Kissat collect.c. Preserve Vivify and binary semantic removal, root-false rewrites and Reduce watermark. No rule admission, layout, proof or model change.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        '40 fixed states distinguish sparse collection from default/Vivify flush across redundancy, all root positions and preexisting retry marks; ten expected parent advice differences, no crash. Pin BVE marks, BCE removal stamps, score notifications, repeated retirement and binary control. Three full suites; additional certificates/B/C and causal/runtime gates still required. No fuzz or promotion authority.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
