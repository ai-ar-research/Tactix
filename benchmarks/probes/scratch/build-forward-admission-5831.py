#!/usr/bin/env python3
"""Independent immutable-ready Release/BC/trace builds for the C2 producer fix."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-forward-admission-5830'
logs = root / 'benchmarks/runs/20260908-5830-forward-admission/logs'
for number, name, trace in [(5830, 'release', False), (5831, 'bc', False), (5832, 'trace', True)]:
    build = root / f'build/pin-{number}-forward-admission-{name}'
    assert not build.exists()
    configure = ['cmake', '-S', source, '-B', build, '-DCMAKE_BUILD_TYPE=Release']
    if trace:
        configure.append('-DTACTIX_ENABLE_EVENT_TRACE=ON')
    for stage, command in [('configure', configure), ('build', ['cmake', '--build', build, '-j8']),
                           ('ctest', ['ctest', '--test-dir', build, '--output-on-failure', '-j8'])]:
        path = logs / f'{name}-{stage}.log'
        with path.open('w') as stream:
            result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
        assert result.returncode == 0, (name, stage, path.read_text()[-8000:])
        print(name, stage, 'PASS', flush=True)
