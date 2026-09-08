#!/usr/bin/env python3
"""Build diagnostic-only independent copies and run fixed Tactix tests."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5456-factor-observer'
t=root/'.claude/worktrees/runtime-factor-observer-5456'
k=root/'tools/solvers/variants/kissat-factor-observer-5456'
b=root/'build/pin-5456-factor-observer-trace'
assert not b.exists()
def build(arm):
    commands=([
      ('configure',['cmake','-S',t,'-B',b,'-DCMAKE_BUILD_TYPE=Release','-DTACTIX_ENABLE_EVENT_TRACE=ON'],root),
      ('build',['cmake','--build',b,'-j8'],root),
      ('ctest',['ctest','--test-dir',b,'--output-on-failure','-j8'],root)] if arm=='tactix' else [
      ('configure',['./configure'],k),('build',['make','-j8'],k)])
    for stage,args,cwd in commands:
        log=c/'logs'/f'{stage}-{arm}.log'
        assert not log.exists()
        with log.open('w') as out:
            r=subprocess.run(args,cwd=cwd,stdout=out,stderr=subprocess.STDOUT)
        if r.returncode:
            print(log.read_text(),flush=True)
            raise RuntimeError(f'{arm} {stage} failed: {r.returncode}')
        print(f'{arm} {stage}: PASS',flush=True)
with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(build,('tactix','kissat')))
