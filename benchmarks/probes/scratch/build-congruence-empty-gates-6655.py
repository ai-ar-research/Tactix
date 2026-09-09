#!/usr/bin/env python3
"""Parent-negative microcases, then three independent candidate build/test pins."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260909-6654-congruence-empty-gates-build'
parent = root / 'build/pin-6579-material-schedule-release'
test = root / '.claude/worktrees/runtime-congruence-empty-gates-6654/tests/test_congruence_empty_gates.cpp'
output = c / 'test-parent-empty-gates'
assert not output.exists()
command = ['c++', '-std=c++20', '-O2', '-fno-exceptions', '-fno-rtti',
           '-I' + str(root / '.claude/worktrees/runtime-material-schedule-6579/src'),
           '-I' + str(root / '.claude/worktrees/runtime-material-schedule-6579/include'),
           str(test), *map(str, sorted((parent / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))), '-o', str(output)]
for name, args in [('parent-compile', command), ('parent-negative', [str(output), '--expect-parent'])]:
    with (c / 'logs' / (name + '.log')).open('x') as out:
        result = subprocess.run(args, cwd=c, stdout=out, stderr=subprocess.STDOUT)
    assert result.returncode == 0, name
    print(name, 'PASS', flush=True)
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
changes = {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-congruence-empty-gates-6654',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-material-schedule-6579',
    '20260909-6231-substitute-retry-build': '20260909-6654-congruence-empty-gates-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6654-congruence-empty-gates-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}": "{'src/rules/congruence/congruence.cpp', 'tests/test_congruence_empty_gates.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Remove the no-gates early return so binary-only equivalence rounds reach existing second dense cleanup. Unit-only lazy cleanup remains controlled by neq; no new primitive, policy or public-guard change.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        'Frozen parent exposes32 cleanup failures while32 controls pass.64 fixed cases cover public/direct APIs, DRAT/LRAT, import order, duplicates, entry versus same-round units and binary-equivalence versus unit-only rounds. Full independent certificates, B/C and Rank49/full50 runtime gates still required. No fuzz or promotion authority.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
