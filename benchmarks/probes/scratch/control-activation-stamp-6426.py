#!/usr/bin/env python3
"""Old actual regression fails, new actual parser stamps match frozen native."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6426-activation-stamp-controls'; assert not c.exists(); c.mkdir()
authority=root/'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json'
b=json.loads(authority.read_text()); source=root/b['source']
for rel,digest in b['files'].items(): assert sha(source/rel)==digest
native_path=root/'benchmarks/runs/20260909-6423-activation-stamp-control/summary.json'
native=json.loads(native_path.read_text())
for group in ('fixtures','input_objects'):
    for rel,digest in native[group].items(): assert sha(root/rel)==digest
outputs={}; commands={}; inputs={}
specs=(('parent','6383-vivify-count-cache',source/'tests/test_activation_stamp.cpp',['--expect-old-missing']),
       ('candidate','6424-activation-stamp',Path(__file__).with_name('test-activation-stamps-6422.cpp'),[]))
for arm,build,fixture,args in specs:
    objects=sorted((root/('build/pin-'+build+'-release/CMakeFiles/tactix_lib.dir')).rglob('*.o'))
    command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,objects),'-o',str(c/(arm+'-fixture'))]
    with (c/(arm+'-build.log')).open('x') as log: subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    p=subprocess.run([str(c/(arm+'-fixture')),*args],cwd=c,text=True,capture_output=True)
    (c/(arm+'.log')).write_text(p.stdout+p.stderr); assert p.returncode==0,(arm,p.returncode,p.stderr)
    outputs[arm]=p.stdout; commands[arm]=command
    for path in [fixture,*objects]: inputs[str(path.relative_to(root))]=sha(path)
assert outputs['parent']=='activation-stamp cases=16 observations=64 missing=64\n'
oracles={tuple(map(int,line.split('\t')[1:3])):list(map(int,line.split('\t')[3:])) for line in native['outputs']['kissat'].splitlines()}
rows=[]
for line in outputs['candidate'].splitlines():
    reverse,declared,attached,stage,ext,stamp,queue_stamp=map(int,line.split('\t')[1:])
    oracle=oracles[reverse,stage]; assert [ext,stamp,queue_stamp]==oracle
    rows.append(dict(reverse=reverse,declared=declared,attached=attached,stage=stage,external=ext,stamp=stamp,queue_stamp=queue_stamp))
assert len(rows)==16
for kind,count in (('release',108),('bc',108),('trace',107)):
    assert f'100% tests passed out of {count}' in (authority.parent/'logs'/f'{kind}-ctest.log').read_text()
(c/'summary.json').write_text(json.dumps(dict(parent_misses=64,native_matches=16,rows=rows,commands=commands,outputs=outputs,
    hashes={str(authority.relative_to(root)):sha(authority),str(native_path.relative_to(root)):sha(native_path),**inputs},
    limitation='General activation producer repaired on actual fixed inputs; both attachment paths and header holes agree with native. No Rank45, timing, complete model/proof gate or source promotion claim.'),indent=2)+'\n')
print('PASS parent misses64/64; candidate/native stamps and queue counters match16/16; full108Release twice107trace')
