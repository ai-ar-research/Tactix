#!/usr/bin/env python3
"""Independent continuation pins and full suites, without touching submitted builds."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-forward-unit-5798'
logs = root / 'benchmarks/runs/20260908-5798-forward-unit/logs'
def build(arm, number):
    directory = root / f'build/pin-{number}-forward-unit-{arm}'
    if arm != 'release':
        assert not directory.exists()
    commands = [('configure', ['cmake', '-S', source, '-B', directory,
        '-DCMAKE_BUILD_TYPE=Release', '-DTACTIX_ENABLE_EVENT_TRACE=' + ('ON' if arm == 'trace' else 'OFF')]),
        ('build', ['cmake', '--build', directory, '-j8']),
        ('ctest', ['ctest', '--test-dir', directory, '--output-on-failure', '-j8'])]
    for stage, command in commands:
        path = logs / f'{stage}-final-{arm}.log'
        assert not path.exists()
        with path.open('w') as stream:
            result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            print(path.read_text(), flush=True)
            raise RuntimeError(f'{arm} {stage}: {result.returncode}')
        print(f'{arm} {stage}: PASS', flush=True)
with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(lambda pair: build(*pair), [('release', 5798), ('bc', 5799), ('trace', 5800)]))
