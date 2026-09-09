#!/usr/bin/env python3
"""Link identical fixed fixtures against actual immutable predecessor/new objects."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6448-shared-blocker-build'
target=c/'control-6450.json'; assert not target.exists()
source=root/'.claude/worktrees/runtime-shared-blocker-6448'
logs={}; outputs={}; bindings={}; generated={}
def run(name,command):
    p=subprocess.run(command,cwd=c,text=True,capture_output=True)
    log=c/'logs'/f'control6450-{name}.json'; assert not log.exists()
    log.write_text(json.dumps(dict(command=command,exit_code=p.returncode,
        stdout=p.stdout,stderr=p.stderr),indent=2)+'\n')
    logs[str(log.relative_to(root))]=sha(log)
    assert p.returncode==0,(name,p.returncode,p.stderr[-2000:])
    return p.stdout
for arm,stem in (('previous','6424-activation-stamp'),('candidate','6448-shared-blocker')):
    build=root/f'build/pin-{stem}-release'
    objects=sorted((build/'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
    bindings.update({str(p.relative_to(root)):sha(p) for p in objects})
    outputs[arm]={}
    for name in ('test_shared_blocker','test_propagation_cursor','test_kernel'):
        fixture=source/'tests'/f'{name}.cpp'; bindings[str(fixture.relative_to(root))]=sha(fixture)
        exe=c/f'control6450-{arm}-{name}'; assert not exe.exists()
        command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O3','-DNDEBUG',
            '-fno-exceptions','-fno-rtti','-I'+str(source/'src'),'-I'+str(source/'include'),
            str(fixture),*map(str,objects),'-o',str(exe)]
        run(arm+'-'+name+'-link',command)
        outputs[arm][name]=run(arm+'-'+name+'-run',[str(exe)])
    obj=build/'CMakeFiles/tactix_lib.dir/src/kernel/kernel.cpp.o'
    symbols=run(arm+'-symbols',['nm','-S','--size-sort','-C',str(obj)])
    generated[arm]=[line for line in symbols.splitlines() if 'kernel_propagate(' in line or 'propagate_literal<' in line]
assert outputs['previous']==outputs['candidate'],outputs
target.write_text(json.dumps(dict(outputs=outputs,generated_symbols=generated,
    hashes=bindings,logs=logs,
    conclusion='Actual old/new mixed-width blocker180 cases and propagation-cursor224 cases have exact state digests. Existing kernel tests pass both actual object sets. No whole-solver runtime or final-proof acceptance claim.'),indent=2)+'\n')
print(json.dumps(dict(outputs=outputs,generated_symbols=generated),indent=2))
