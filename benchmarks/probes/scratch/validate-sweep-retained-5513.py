#!/usr/bin/env python3
"""Final suites after fixture corrections; preserve initial failed expectations."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import shlex

root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-sweep-retained-5510'
logs = root / 'benchmarks/runs/20260908-5510-sweep-retained/logs'

def run(name, command):
    path = logs / (name + '.log')
    assert not path.exists()
    with path.open('w') as stream:
        result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print(path.read_text(), flush=True)
        raise RuntimeError(f'{name}: {result.returncode}')
    print(name + ': PASS', flush=True)

def build(arm, number):
    directory = root / f'build/pin-{number}-sweep-retained-{arm}'
    if arm == 'trace':
        assert not directory.exists()
        run('configure-trace', ['cmake', '-S', source, '-B', directory,
            '-DCMAKE_BUILD_TYPE=Release', '-DTACTIX_ENABLE_EVENT_TRACE=ON'])
    run('build-final-' + arm, ['cmake', '--build', directory, '-j8'])
    run('ctest-final-' + arm, ['ctest', '--test-dir', directory,
        '--output-on-failure', '-j8'])

with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(lambda pair: build(*pair), [('release', 5510), ('bc', 5512), ('trace', 5511)]))

old = root / 'build/pin-5473-sweep-cursor-release'
command = shlex.split((old / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((old / 'tests' / arg).resolve()) for arg in command
           if arg.endswith('.o') and 'test_kernel.cpp.o' not in arg]
target = root / 'benchmarks/probes/scratch/sweep-retained-old-final-5513'
assert not target.exists()
run('compile-old-final', [command[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-sweep-cursor-5473/src'),
    str(source / 'tests/test_sweep_retained.cpp'), *objects, '-o', str(target)])
run('old-final-counterexample', [target, '--old-counterexample'])
