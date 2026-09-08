#!/usr/bin/env python3
"""Independent Release and trace builds, with full fixed suites."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-sweep-small-5522'
logs = root / 'benchmarks/runs/20260908-5522-sweep-small/logs'
def build(arm, number):
    directory = root / f'build/pin-{number}-sweep-small-{arm}'
    assert not directory.exists()
    commands = [('configure', ['cmake', '-S', source, '-B', directory,
        '-DCMAKE_BUILD_TYPE=Release', '-DTACTIX_ENABLE_EVENT_TRACE=' + ('ON' if arm == 'trace' else 'OFF')]),
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
with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(lambda pair: build(*pair), [('release', 5522), ('trace', 5523), ('bc', 5524)]))
