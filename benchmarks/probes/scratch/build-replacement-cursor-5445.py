#!/usr/bin/env python3
"""Independent Release builds and fixed tests; no real-instance solver runs."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-replacement-cursor-5445'
logs = root / 'benchmarks/runs/20260907-5445-replacement-cursor/logs'

def build(arm, number):
    directory = root / f'build/pin-{number}-replacement-cursor-{arm}'
    assert not directory.exists()
    commands = [
        ('configure', ['cmake', '-S', source, '-B', directory, '-DCMAKE_BUILD_TYPE=Release']),
        ('build', ['cmake', '--build', directory, '-j8']),
        ('ctest', ['ctest', '--test-dir', directory, '--output-on-failure', '-j8']),
    ]
    for stage, command in commands:
        path = logs / f'{stage}-{arm}.log'
        assert not path.exists()
        with path.open('w') as out:
            result = subprocess.run(command, cwd=root, stdout=out, stderr=subprocess.STDOUT)
        if result.returncode:
            print(path.read_text(), flush=True)
            raise RuntimeError(f'{arm} {stage}: {result.returncode}')
        print(f'{arm} {stage}: PASS', flush=True)

with ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(lambda arg: build(*arg), [('release', 5445), ('bc', 5446)]))
