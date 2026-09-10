#!/usr/bin/env python3
"""Compile an actual pinned native caller fixture; never alter its source pin."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6732-reduce-watermark-native'
c.mkdir(exist_ok=False)
native=root/'tools/solvers/variants/kissat-macro-heap-6090'
fixture=Path(__file__).with_name('reduce-watermark-native-6732.c')
commands=[('compile',['gcc','-DNDEBUG','-O2','-I'+str(native/'src'),str(fixture),str(native/'build/libkissat.a'),'-lm','-o',str(c/'native')]),
          ('fixture',[str(c/'native')])]
for name,command in commands:
    with (c/(name+'.log')).open('x') as out:
        r=subprocess.run(command,cwd=c,stdout=out,stderr=subprocess.STDOUT)
    assert r.returncode==0,(name,r.returncode)
    print(name,'PASS',flush=True)
paths=[fixture,Path(__file__),native/'build/libkissat.a',c/'native',*(c.glob('*.log')),
       *(p for p in (native/'src').iterdir() if p.is_file() and p.suffix in ('.h','.c'))]
with (c/'summary.json').open('x') as out:
    json.dump(dict(cases=3,actual_native_reduce=True,commands=commands,
        hashes={str(p.relative_to(root)):sha(p) for p in paths},
        scope='Actual Kissat Reduce call: promoted and retired historical boundary resets the following irredundant cursor; originally irredundant control retains it. Genuine prefix remains unchanged. Structural fixed microtest, not a benchmark or certificate claim.'),out,indent=2)
    out.write('\n')
