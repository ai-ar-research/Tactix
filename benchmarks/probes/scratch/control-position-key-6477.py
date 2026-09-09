#!/usr/bin/env python3
"""Actual immutable old/new object controls for order and allocation payload."""
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6475-position-key-build'
target=c/'control-6477.json'; assert not target.exists()
source=root/'.claude/worktrees/runtime-position-key-6475'
manifest=json.loads((c/'manifest.json').read_text())
for rel,digest in manifest['files'].items(): assert sha(source/rel)==digest
logs={}; outputs={}; bindings={}; measurements={}
def run(name,command):
    p=subprocess.run(command,cwd=c,text=True,capture_output=True)
    log=c/'logs'/f'control6477-{name}.json'; assert not log.exists()
    log.write_text(json.dumps(dict(command=command,exit_code=p.returncode,
        stdout=p.stdout,stderr=p.stderr),indent=2)+'\n')
    logs[str(log.relative_to(root))]=sha(log)
    assert p.returncode==0,(name,p.returncode,p.stderr[-2000:])
    return p.stdout
for arm,stem in (('previous','6424-activation-stamp'),('candidate','6475-position-key')):
    build=root/f'build/pin-{stem}-release'
    authority=root/f'benchmarks/runs/20260909-{stem}-build/manifest.json'
    a=json.loads(authority.read_text())
    for rel,digest in a['files'].items(): assert sha(root/a['source']/rel)==digest
    assert sha(build/'tactix')==a['binaries'][str((build/'tactix').relative_to(root))]
    bindings[str(authority.relative_to(root))]=sha(authority)
    objects=sorted((build/'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
    bindings.update({str(p.relative_to(root)):sha(p) for p in objects})
    outputs[arm]={}
    for name in ('test_position_keys','test_kernel','test_walk'):
        fixture=source/'tests'/f'{name}.cpp'; bindings[str(fixture.relative_to(root))]=sha(fixture)
        exe=c/f'control6477-{arm}-{name}'; assert not exe.exists()
        command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O3','-DNDEBUG',
            '-fno-exceptions','-fno-rtti','-I'+str(source/'src'),'-I'+str(source/'include')]
        if arm=='previous' and name=='test_position_keys': command+=['-DTACTIX_POSITION_KEY_LEGACY_CONTROL=1']
        command += [str(fixture),*map(str,objects),'-o',str(exe)]
        run(arm+'-'+name+'-link',command)
        outputs[arm][name]=run(arm+'-'+name+'-run',[str(exe)])
    line=outputs[arm]['test_position_keys'].strip()
    found=re.fullmatch(r'position-key cases=(\d+) sorted=(\d+) refs=(\d+) digest=([0-9a-f]{16}) bytes=(\d+)',line)
    assert found,line
    cases,sorts,refs,digest,allocated=found.groups()
    measurements[arm]=dict(cases=int(cases),sorted_cases=int(sorts),sorted_refs=int(refs),
        digest=digest,allocated_bytes=int(allocated))
    assert int(cases)==480 and int(sorts)>0
for name in ('test_kernel','test_walk'): assert outputs['candidate'][name]==outputs['previous'][name]
old,new=measurements['previous'],measurements['candidate']
for key in ('cases','sorted_cases','sorted_refs','digest'): assert old[key]==new[key]
assert old['allocated_bytes']==20*old['sorted_refs']
assert new['allocated_bytes']==12*new['sorted_refs']
target.write_text(json.dumps(dict(outputs=outputs,measurements=measurements,
    hashes=bindings,logs=logs,
    conclusion='480 actual-object cases preserve complete stable order, arena and aliases; existing kernel and Walk tests agree. Internal sort payload20n to12n bytes, two key buffers halve. No fullsolver RSS, proof acceptance or runtime gain claim.'),indent=2)+'\n')
print(json.dumps(measurements,indent=2))
