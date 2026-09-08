#!/usr/bin/env python3
"""Final full suites after complete empty-environment routing and budget fixture."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
logs = root / 'benchmarks/runs/20260908-5522-sweep-small/logs'
def run(name, command):
    path = logs / (name + '.log')
    assert not path.exists()
    with path.open('w') as stream:
        result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print(path.read_text(), flush=True)
        raise RuntimeError(name + ': ' + str(result.returncode))
    print(name + ': PASS', flush=True)
def build(arm, number):
    directory = root / f'build/pin-{number}-sweep-small-{arm}'
    run('build-final-' + arm, ['cmake', '--build', directory, '-j8'])
    run('ctest-final-' + arm, ['ctest', '--test-dir', directory, '--output-on-failure', '-j8'])
with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(lambda pair: build(*pair), [('release', 5522), ('trace', 5523), ('bc', 5524)]))
old = root / 'build/pin-5510-sweep-retained-release'
command = shlex.split((old / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((old / 'tests' / arg).resolve()) for arg in command
           if arg.endswith('.o') and 'test_kernel.cpp.o' not in arg]
target = root / 'benchmarks/probes/scratch/small-sweep-old-final-5527'
assert not target.exists()
run('compile-old-final', [command[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-sweep-retained-5510/src'),
    str(root / '.claude/worktrees/runtime-sweep-small-5522/tests/test_sweep_small_env.cpp'),
    *objects, '-o', str(target)])
run('old-final-counterexample', [target, '--old-counterexample'])
