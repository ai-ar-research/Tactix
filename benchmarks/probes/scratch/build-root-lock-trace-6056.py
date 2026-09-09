#!/usr/bin/env python3
"""Build the separate corrected diagnostic, preserving all6049 build evidence."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
source=root/'.claude/worktrees/runtime-reduce-root-lock-trace-6054'
build=root/'build/pin-6054-reduce-root-lock-trace'; assert not build.exists()
logs=root/'benchmarks/runs/20260908-6054-root-lock-trace-controls/logs'
for stage,command in [('configure',['cmake','-S',source,'-B',build,'-DCMAKE_BUILD_TYPE=Release','-DTACTIX_ENABLE_EVENT_TRACE=ON']),
                      ('build',['cmake','--build',build,'-j8']),
                      ('ctest',['ctest','--test-dir',build,'--output-on-failure','-j1'])]:
    path=logs/(stage+'.log');assert not path.exists()
    with path.open('w') as f:r=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT)
    assert r.returncode==0,(stage,path.read_text()[-10000:])
    print(stage,'PASS',flush=True)
