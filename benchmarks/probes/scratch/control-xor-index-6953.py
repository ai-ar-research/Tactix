#!/usr/bin/env python3
"""Test private unfinished-index behavior with actual old/new translation units."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6950-xor-presence-build'
target = c/'index-control-6953.json'; assert not target.exists()
control = c/'control-6951.json'; m = json.loads(control.read_text())
for group in ('hashes','artifacts','parent_objects','logs'):
    for rel,digest in m[group].items(): assert sha(root/rel) == digest,rel
fixture = root/'benchmarks/probes/scratch/test-xor-index-6952.cpp'
objects = [root/p for p in m['parent_objects'] if not p.endswith('/congruence/congruence.cpp.o')]
hashes = {str(p.relative_to(root)):sha(p) for p in [Path(__file__),fixture,control]}
outputs = {}
for arm, name in (('previous','reduce-watermark-6730'),('candidate','xor-presence-6950')):
    tree = root/('.claude/worktrees/runtime-'+name)
    exe = c/(arm+'-index-6953'); assert not exe.exists()
    command = ['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O3','-DNDEBUG',
        '-fno-exceptions','-fno-rtti','-I'+str(tree/'src'),'-I'+str(tree/'include'),
        '-include',str(tree/'src/rules/congruence/congruence.cpp'),str(fixture),*map(str,objects),'-o',str(exe)]
    for action,cmd in (('build',command),('run',[str(exe)])):
        p = subprocess.run(cmd,capture_output=True,text=True)
        log = c/'logs'/f'{arm}-index-{action}-6953.json'
        with log.open('x') as out:
            json.dump(dict(command=cmd,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr),out,indent=2); out.write('\n')
        hashes[str(log.relative_to(root))] = sha(log)
        assert p.returncode == 0,(arm,action,p.stderr)
        if action == 'run': outputs[arm] = p.stdout
    hashes[str(exe.relative_to(root))] = sha(exe)
assert outputs['previous'] == outputs['candidate']
with target.open('x') as out:
    json.dump(dict(hashes=hashes,outputs=outputs,scope='1024 complete/unfinished sign-subset indices plus two actual constructor cuts (budget and stop). Full ordered XOR payload/ticks/interrupt digests agree. Actual source included, no copied index or algorithm.'),out,indent=2);out.write('\n')
print(outputs)
