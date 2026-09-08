#!/usr/bin/env python3
"""Build diagnostic clones independently and run the full Tactix trace suite."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5956-bve-full-heap-controls'
def run(name,commands,cwd):
    for stage,cmd in commands:
        log=c/'logs'/f'{name}-{stage}.log';assert not log.exists()
        with log.open('w') as f:r=subprocess.run(cmd,cwd=cwd,stdout=f,stderr=subprocess.STDOUT)
        assert r.returncode==0,(name,stage,log.read_text()[-8000:])
        print(name,stage,'PASS',flush=True)
source=root/'.claude/worktrees/runtime-bve-full-heap-5956'
build=root/'build/pin-5956-bve-full-heap-trace'
with ThreadPoolExecutor(max_workers=2) as pool:
    futures=[pool.submit(run,'tactix',[
        ('configure',['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release','-DTACTIX_ENABLE_EVENT_TRACE=ON']),
        ('build',['cmake','--build',str(build),'-j8']),
        ('ctest',['ctest','--test-dir',str(build),'--output-on-failure','-j1'])],root),
        pool.submit(run,'kissat',[('configure',['./configure']),('build',['make','-j8'])],root/'tools/solvers/variants/kissat-bve-full-heap-5956')]
    for future in futures:future.result()
