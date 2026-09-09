#!/usr/bin/env python3
"""Supplement frozen6424 tests with the newly observed unit-first parse shape."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6430-unit-first-stamp-control'; assert not c.exists(); c.mkdir()
t=root/'.claude/worktrees/runtime-activation-stamp-6424'; n=root/'tools/solvers/variants/kissat-macro-heap-6090'
fixture=Path(__file__).with_name('test-unit-first-stamps-6430.cpp'); native=Path(__file__).with_name('test-native-unit-first-stamps-6430.c')
commands={}; inputs={str(p.relative_to(root)):sha(p) for p in (fixture,native)}
for arm,build in (('parent','6383-vivify-count-cache'),('candidate','6424-activation-stamp')):
    objects=sorted((root/('build/pin-'+build+'-release/CMakeFiles/tactix_lib.dir')).rglob('*.o'))
    commands[arm]=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(t/'src'),'-I'+str(t/'include'),str(fixture),*map(str,objects),'-o',str(c/(arm+'-fixture'))]
    inputs.update({str(p.relative_to(root)):sha(p) for p in objects})
library=n/'build/libkissat.a'; inputs[str(library.relative_to(root))]=sha(library)
commands['native']=['gcc','-O2','-I'+str(n/'src'),'-I'+str(n/'build'),str(native),str(library),'-lm','-o',str(c/'native-fixture')]
def run(arm):
    with (c/(arm+'-build.log')).open('x') as log: subprocess.run(commands[arm],stdout=log,stderr=subprocess.STDOUT,check=True)
    p=subprocess.run([str(c/(arm+'-fixture'))]+(['--expect-old-missing'] if arm=='parent' else []),cwd=c,text=True,capture_output=True)
    (c/(arm+'.log')).write_text(p.stdout+p.stderr); assert p.returncode==0,(arm,p.returncode,p.stderr)
    return arm,p.stdout
with ThreadPoolExecutor(max_workers=3) as pool: outputs=dict(pool.map(run,commands))
assert outputs==dict(parent='unit-first observations=16 missing=16\n',candidate='unit-first observations=16 missing=0\n',native='native unit-first observations=4 stamp=1\n')
(c/'summary.json').write_text(json.dumps(dict(outputs=outputs,commands=commands,hashes=inputs,
    limitation='Fixed units-before-clause control; no source/pin edits or real solver invocation. Confirms numeric producer on Rank45-like shape, not its complete repaired trajectory or runtime.'),indent=2)+'\n')
print(outputs)
