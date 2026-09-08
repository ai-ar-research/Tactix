#!/usr/bin/env python3
"""Complete unsubmitted builds, retaining the first cleanup-failure log."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-forward-caller-5849'
logs = root / 'benchmarks/runs/20260908-5849-forward-caller/logs'
builds = [('release', 5849, []), ('bc', 5850, []),
          ('trace', 5851, ['-DTACTIX_ENABLE_EVENT_TRACE=ON']),
          ('journal', 5854, ['-DTACTIX_ENABLE_RULE_JOURNAL=ON'])]
for name, number, options in builds:
    build = root / f'build/pin-{number}-forward-caller-{name}'
    for stage, command in [
        ('configure-final', ['cmake', '-S', source, '-B', build, '-DCMAKE_BUILD_TYPE=Release', *options]),
        ('build-final', ['cmake', '--build', build, '-j8']),
        ('ctest-final', ['ctest', '--test-dir', build, '--output-on-failure', '-j1'])]:
        path = logs / f'{name}-{stage}.log'
        assert not path.exists()
        with path.open('w') as stream:
            result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
        assert result.returncode == 0, (name, stage, path.read_text()[-10000:])
        print(name, stage, 'PASS', flush=True)
