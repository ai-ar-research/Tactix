#!/usr/bin/env python3
"""Retry an unsubmitted fixture compile after correcting the ReservedTrail API."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
logs = root / 'benchmarks/runs/20260908-5641-propagation-cursor/logs'
def build(pair):
    arm, number = pair
    directory = root / f'build/pin-{number}-propagation-cursor-{arm}'
    for stage, command in [
        ('build', ['cmake', '--build', directory, '-j8']),
        ('ctest', ['ctest', '--test-dir', directory, '--output-on-failure', '-j8'])]:
        log = logs / f'{stage}-{arm}.r1.log'
        assert not log.exists()
        with log.open('w') as stream:
            result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            print(log.read_text(), flush=True)
            raise RuntimeError((arm, stage, result.returncode))
        print(arm, stage, 'PASS', flush=True)
with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(build, [('release', 5641), ('trace', 5642), ('bc', 5643)]))
