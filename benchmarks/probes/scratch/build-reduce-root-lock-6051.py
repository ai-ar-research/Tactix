#!/usr/bin/env python3
"""Four independent builds and full suites for the isolated root-lock repair."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
source=root/'.claude/worktrees/runtime-reduce-root-lock-6049'
logs=root/'benchmarks/runs/20260908-6049-reduce-root-lock/logs'
for name,number,options in [('release',6049,[]),('bc',6050,[]),
    ('trace',6051,['-DTACTIX_ENABLE_EVENT_TRACE=ON']),
    ('journal',6052,['-DTACTIX_ENABLE_RULE_JOURNAL=ON'])]:
    build=root/f'build/pin-{number}-reduce-root-lock-{name}'
    assert not build.exists()
    for stage,command in [('configure',['cmake','-S',source,'-B',build,'-DCMAKE_BUILD_TYPE=Release',*options]),
                          ('build',['cmake','--build',build,'-j8']),
                          ('ctest',['ctest','--test-dir',build,'--output-on-failure','-j1'])]:
        path=logs/f'{name}-{stage}.log'; assert not path.exists()
        with path.open('w') as f: r=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT)
        assert r.returncode==0,(name,stage,path.read_text()[-12000:])
        print(name,stage,'PASS',flush=True)
