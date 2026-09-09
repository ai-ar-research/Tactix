#!/usr/bin/env python3
"""Build read-only scheduling adapters; require unchanged ordinary Release."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6350-transitive-schedule-build'
parents=json.loads((c/'parents.json').read_text());assert not (c/'manifest.json').exists()
source=root/'.claude/worktrees/runtime-transitive-schedule-6350'
native=root/'tools/solvers/variants/kissat-transitive-schedule-6350'
def run(name,command,cwd=None):
    with (c/'logs'/(name+'.log')).open('x') as log:
        subprocess.run(command,cwd=cwd,stdout=log,stderr=subprocess.STDOUT,check=True)
    print(name,'PASS',flush=True)
def build(kind):
    if kind=='native':
        run('native-configure',['./configure'],native)
        run('native-build',['make','-j4'],native)
        return str((native/'build/kissat').relative_to(root)),sha(native/'build/kissat')
    out=root/('build/pin-6350-transitive-schedule-'+kind);assert not out.exists()
    run(kind+'-configure',['cmake','-S',str(source),'-B',str(out),'-DCMAKE_BUILD_TYPE=Release',
        '-DTACTIX_ENABLE_CONFLICT_STOP=OFF','-DTACTIX_ENABLE_EVENT_TRACE='+('ON' if kind=='trace' else 'OFF')])
    run(kind+'-build',['cmake','--build',str(out),'-j4'])
    run(kind+'-ctest',['ctest','--test-dir',str(out),'--output-on-failure','-j1'])
    count=105 if kind=='release' else 104
    assert f'100% tests passed out of {count}' in (c/'logs'/(kind+'-ctest.log')).read_text()
    return str((out/'tactix').relative_to(root)),sha(out/'tactix')
for compiler,kind in (('gcc','c'),('g++','cpp')):
    exe=c/('writer-'+kind);output=c/('writer-'+kind+'.tsv')
    run(kind+'-writer-build',[compiler,'-O2','-Wall','-Wextra','-Werror','-I'+str(Path(__file__).parent),
        str(Path(__file__).with_name('test-transitive-schedule-6351.c')),'-o',str(exe)])
    run(kind+'-writer',[str(exe),str(output)])
assert (c/'writer-c.tsv').read_bytes()==(c/'writer-cpp.tsv').read_bytes()
with ThreadPoolExecutor(max_workers=3) as pool:binaries=dict(pool.map(build,('release','trace','native')))
assert binaries['build/pin-6350-transitive-schedule-release/tactix']=='33a94aec8df9a3be4f2a902e13d59dcc1527a412302f646140bab2ec5c4193e4'
files={};patches={}
for path,parent in parents['parents'].items():
    new=root/path;old=root/parent['source']
    for rel,digest in parent['files'].items():assert sha(old/rel)==digest,rel
    current={str(p.relative_to(new)):sha(p) for folder in ('src','include','tests') if (new/folder).exists()
             for p in (new/folder).rglob('*') if p.is_file()}
    changed={p for p,digest in current.items() if parent['files'].get(p)!=digest}
    expected={'src/policies/default_policy.cpp','src/policies/transitive-schedule-6350.h'} if new==source else {'src/transitive.c','src/transitive-schedule-6350.h','src/makefile'}
    assert changed==expected,changed
    if new==native:assert (new/'src/makefile').read_text()==(old/'src/makefile').read_text().replace(str(old),str(new))
    patch=''
    for rel in sorted(changed-{'src/makefile'}):
        a,b=old/rel,new/rel
        patch+=''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],b.read_text().splitlines(True),
            fromfile='a/'+rel if a.exists() else '/dev/null',tofile='b/'+rel,n=0))
    p=c/('candidate.patch' if new==source else 'native.patch');p.write_text(patch)
    patches[p.name]=sha(p);files[path]=current
(c/'manifest.json').write_text(json.dumps(dict(files=files,binaries=binaries,patches=patches,
    parents_sha256=sha(c/'parents.json'),release_exact_parent=True,
    logs={str(p.relative_to(c)):sha(p) for p in (c/'logs').iterdir()},
    tests='105Release104trace; shared actual C/C++ writer filter/framing/tie-key/repeated-snapshot fixture outputs byte-identical. Full actual-adapter ordinary parent trace/proof replay still required.',
    limitation='Read-only trace scheduling inputs/outputs; no public API, guard, sort, budget, proof or production change. Tactix priority column is0 because its comparator has no such input; native is actual flag.'),indent=2)+'\n')
print(json.dumps(binaries,indent=2))
