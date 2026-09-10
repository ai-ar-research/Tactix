#!/usr/bin/env python3
"""Actual old/new kernel controls; every shared object's source is unchanged."""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6902-watch-rebuild-build'
target = c/'control-6903.json'
assert not target.exists()
parent_manifest = root/'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json'
m = json.loads(parent_manifest.read_text())
parent = root/m['source']
source = root/'.claude/worktrees/runtime-watch-rebuild-6902'
for rel,digest in m['files'].items():
    assert sha(parent/rel) == digest,rel
    if rel != 'src/kernel/kernel.cpp':
        assert sha(source/rel) == digest,rel
pin = root/'build/pin-6730-reduce-watermark-release'
objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
other = [p for p in objects if not str(p).endswith('/kernel/kernel.cpp.o')]
assert objects and len(other) == len(objects)-1
logs = {}
def run(name,command):
    path = c/'logs'/f'{name}-6903.json'
    assert not path.exists()
    p = subprocess.run(command,cwd=c,capture_output=True,text=True)
    with path.open('x') as stream:
        json.dump(dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr),stream,indent=2)
        stream.write('\n')
    logs[str(path.relative_to(root))] = sha(path)
    assert p.returncode == 0,(name,p.returncode,p.stderr[-3000:])
    return p.stdout
outputs = {}
artifacts = {}
for arm,tree in (('previous',parent),('candidate',source)):
    flags = ['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O3','-DNDEBUG',
             '-fno-exceptions','-fno-rtti','-I'+str(tree/'src'),'-I'+str(tree/'include')]
    obj = c/f'{arm}-kernel-6903.o'
    assert not obj.exists()
    run(arm+'-compile',flags+['-c',str(tree/'src/kernel/kernel.cpp'),'-o',str(obj)])
    artifacts[str(obj.relative_to(root))] = sha(obj)
    outputs[arm] = {}
    for name in ('test_watch_rebuild','test_kernel','test_replacement_cursor'):
        exe = c/f'{arm}-{name}-6903'
        assert not exe.exists()
        run(arm+'-'+name+'-link',flags+[str(source/'tests'/f'{name}.cpp'),str(obj),*map(str,other),'-o',str(exe)])
        outputs[arm][name] = run(arm+'-'+name+'-run',[str(exe)])
        artifacts[str(exe.relative_to(root))] = sha(exe)
assert outputs['previous'] == outputs['candidate'],outputs
paths = [Path(__file__),parent_manifest,source/'src/kernel/kernel.cpp',source/'tests/test_watch_rebuild.cpp']
out = dict(outputs=outputs,hashes={str(p.relative_to(root)):sha(p) for p in paths},logs=logs,
           parent_objects={str(p.relative_to(root)):sha(p) for p in objects},artifacts=artifacts,
           conclusion='144 complete physical-image configurations per actual kernel arm, conservative order-bit normalization, empty/binary/mixed/garbage/duplicate lists, connected/drop/flush modes, both flush flags, root values and attached ordered dense views. Existing kernel and replacement-cursor suites agree. Shared objects are from6730 whose entire source/header tree except kernel.cpp is hash-identical. No ABI change. Not final independent proof/runtime/fuzz acceptance.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2);stream.write('\n')
print(json.dumps(outputs,indent=2))
