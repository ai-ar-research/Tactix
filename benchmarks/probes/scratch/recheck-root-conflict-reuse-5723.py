#!/usr/bin/env python3
"""Retain original failed logs, then validate the corrected test contract."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
logs=root/'benchmarks/runs/20260908-5718-root-conflict-reuse/logs'
def check(pair):
    arm,number=pair
    b=root/f'build/pin-{number}-root-conflict-reuse-{arm}'
    for stage,cmd in [('build-r1',['cmake','--build',b,'-j8']),
                      ('ctest-r1',['ctest','--test-dir',b,'--output-on-failure','-j8'])]:
        p=logs/f'{stage}-{arm}.log';assert not p.exists()
        with p.open('w') as out:r=subprocess.run(cmd,cwd=root,stdout=out,stderr=subprocess.STDOUT)
        if r.returncode:print(p.read_text(),flush=True)
        assert r.returncode==0
        print(stage,arm,'PASS',flush=True)
with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(check,[('release',5718),('trace',5719),('bc',5720)]))
