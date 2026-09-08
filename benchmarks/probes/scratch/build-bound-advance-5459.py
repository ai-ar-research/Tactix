#!/usr/bin/env python3
"""Build independent isolated bound-lifecycle candidates and fixed tests."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[3]
source=root/'.claude/worktrees/runtime-bound-advance-5459'
logs=root/'benchmarks/runs/20260907-5459-bound-advance/logs'

def build(spec):
    arm,number=spec
    directory=root/f'build/pin-{number}-bound-advance-{arm}'
    assert not directory.exists()
    configure=['cmake','-S',source,'-B',directory,'-DCMAKE_BUILD_TYPE=Release']
    if arm=='trace': configure.append('-DTACTIX_ENABLE_EVENT_TRACE=ON')
    for stage,command in [('configure',configure),
        ('build',['cmake','--build',directory,'-j6']),
        ('ctest',['ctest','--test-dir',directory,'--output-on-failure','-j6'])]:
        path=logs/f'{stage}-{arm}.log'
        assert not path.exists()
        with path.open('w') as out:
            result=subprocess.run(command,cwd=root,stdout=out,stderr=subprocess.STDOUT)
        if result.returncode:
            print(path.read_text(),flush=True)
            raise RuntimeError(f'{arm} {stage}: {result.returncode}')
        print(f'{arm} {stage}: PASS',flush=True)

with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(build,[('release',5459),('trace',5460),('bc',5461)]))
