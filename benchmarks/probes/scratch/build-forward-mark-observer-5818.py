#!/usr/bin/env python3
"""Build only the new unsubmitted observer; full fixed suite remains required."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-forward-mark-observer-5817'
build = root / 'build/pin-5817-forward-mark-trace'
logs = root / 'benchmarks/runs/20260908-5817-forward-mark-observer/logs'
assert not build.exists()
for name,command in [('configure',['cmake','-S',source,'-B',build,'-DCMAKE_BUILD_TYPE=Release','-DTACTIX_ENABLE_EVENT_TRACE=ON']),
                     ('build',['cmake','--build',build,'-j8']),
                     ('ctest',['ctest','--test-dir',build,'--output-on-failure','-j8'])]:
    path = logs / (name + '.log')
    with path.open('w') as stream:
        r = subprocess.run(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT)
    assert r.returncode == 0,(name,path.read_text()[-6000:])
    print(name,'PASS',flush=True)
