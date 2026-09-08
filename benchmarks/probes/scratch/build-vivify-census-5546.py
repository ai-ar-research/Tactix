#!/usr/bin/env python3
"""Independent Release builds and full suites for the unused-census optimization."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-vivify-census-5546'
logs = root / 'benchmarks/runs/20260908-5546-vivify-census/logs'
def build(arm, number):
    directory = root / f'build/pin-{number}-vivify-census-{arm}'
    assert not directory.exists()
    commands = [('configure', ['cmake', '-S', source, '-B', directory,
        '-DCMAKE_BUILD_TYPE=Release', '-DTACTIX_ENABLE_EVENT_TRACE=OFF']),
        ('build', ['cmake', '--build', directory, '-j8']),
        ('ctest', ['ctest', '--test-dir', directory, '--output-on-failure', '-j8'])]
    for stage, command in commands:
        path = logs / f'{stage}-{arm}.log'
        assert not path.exists()
        with path.open('w') as stream:
            result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            print(path.read_text(), flush=True)
            raise RuntimeError(f'{arm} {stage}: {result.returncode}')
        print(f'{arm} {stage}: PASS', flush=True)
with ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(lambda pair: build(*pair), [('release', 5546), ('bc', 5547)]))
