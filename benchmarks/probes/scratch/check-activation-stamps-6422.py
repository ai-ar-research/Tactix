#!/usr/bin/env python3
"""Actual fixed parser/attach/collector controls; no search or mirrored solver."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6422-activation-stamp-control'; assert not c.exists(); c.mkdir()
t=root/'.claude/worktrees/runtime-compact-stamp-6357'
n=root/'tools/solvers/variants/kissat-macro-heap-6090'
objects=sorted((root/'build/pin-6357-compact-stamp-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
fixtures={a:Path(__file__).with_name(name) for a,name in (
    ('tactix','test-activation-stamps-6422.cpp'),('kissat','test-native-activation-stamps-6422.c'))}
commands=dict(tactix=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(t/'src'),'-I'+str(t/'include'),str(fixtures['tactix']),*map(str,objects),'-o',str(c/'tactix-fixture')],
    kissat=['gcc','-O2','-I'+str(n/'src'),'-I'+str(n/'build'),str(fixtures['kissat']),str(n/'build/libkissat.a'),'-lm','-o',str(c/'kissat-fixture')])
def run(arm):
    with (c/(arm+'-build.log')).open('x') as log: subprocess.run(commands[arm],stdout=log,stderr=subprocess.STDOUT,check=True)
    p=subprocess.run([str(c/(arm+'-fixture'))],cwd=c,text=True,capture_output=True)
    (c/(arm+'.log')).write_text(p.stdout+p.stderr)
    assert p.returncode==0,(arm,p.returncode,p.stderr)
    return arm,p.stdout
with ThreadPoolExecutor(max_workers=2) as pool: outputs=dict(pool.map(run,commands))
native={tuple(map(int,line.split('\t')[1:3])):list(map(int,line.split('\t')[3:])) for line in outputs['kissat'].splitlines()}
assert len(native)==4
rows=[]
for line in outputs['tactix'].splitlines():
    a=list(map(int,line.split('\t')[1:])); reverse,declared,attached,stage,ext,stamp,queue_stamp=a
    oracle=native[reverse,stage]; assert ext==oracle[0]
    rows.append(dict(reverse=reverse,declared=declared,attached=attached,stage=stage,external=ext,
        tactix_stamp=stamp,kissat_stamp=oracle[1],tactix_queue_stamp=queue_stamp,kissat_queue_stamp=oracle[2],equal=stamp==oracle[1]))
assert len(rows)==16
(c/'summary.json').write_text(json.dumps(dict(commands=commands,rows=rows,outputs=outputs,
    fixtures={str(p.relative_to(root)):sha(p) for p in fixtures.values()},
    input_objects={str(p.relative_to(root)):sha(p) for p in [*objects,n/'build/libkissat.a']},
    limitation='Fixed parser/activation and real compaction, no solver invocation or performance claim. Native has no DIMACS-header-only identities; both Tactix attachment paths are observed, not asserted equivalent. Does not yet attribute Rank45.'),indent=2)+'\n')
print(json.dumps(rows,indent=2))
