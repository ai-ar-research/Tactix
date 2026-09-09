#!/usr/bin/env python3
"""Build both diagnostic configurations, requiring ordinary Release identity."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6413-rank45-schedule-build'
p=json.loads((c/'parents.json').read_text()); source=root/p['candidate']; old=root/p['source']
assert not (c/'manifest.json').exists()
def run(name,command):
    with (c/'logs'/(name+'.log')).open('x') as log:
        subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,cwd=root)
    print(name,'PASS',flush=True)
def build(kind):
    out=root/('build/pin-6413-rank45-schedule-'+kind); assert not out.exists()
    run(kind+'-configure',['cmake','-S',str(source),'-B',str(out),'-DCMAKE_BUILD_TYPE=Release',
        '-DTACTIX_ENABLE_CONFLICT_STOP=OFF','-DTACTIX_ENABLE_EVENT_TRACE='+('ON' if kind=='trace' else 'OFF')])
    run(kind+'-build',['cmake','--build',str(out),'-j4'])
    run(kind+'-ctest',['ctest','--test-dir',str(out),'--output-on-failure','-j1'])
    count=106 if kind=='release' else 105
    assert f'100% tests passed out of {count}' in (c/'logs'/(kind+'-ctest.log')).read_text()
    return str((out/'tactix').relative_to(root)),sha(out/'tactix')
for compiler,kind in (('gcc','c'),('g++','cpp')):
    exe=c/('writer-'+kind); output=c/('writer-'+kind+'.tsv')
    run(kind+'-writer-build',[compiler,'-O2','-Wall','-Wextra','-Werror','-I'+str(Path(__file__).parent),
        str(Path(__file__).with_name('test-transitive-schedule-6351.c')),'-o',str(exe)])
    run(kind+'-writer',[str(exe),str(output)])
assert (c/'writer-c.tsv').read_bytes()==(c/'writer-cpp.tsv').read_bytes()
with ThreadPoolExecutor(max_workers=2) as pool: binaries=dict(pool.map(build,('release','trace')))
assert binaries['build/pin-6413-rank45-schedule-release/tactix']=='00a23b9c8f5ec050cb0a1eb05a903c44496b8fd40c650e62eae7ae182a78a695'
for rel,digest in p['files'].items(): assert sha(old/rel)==digest,rel
files={str(f.relative_to(source)):sha(f) for folder in ('src','include','tests') for f in (source/folder).rglob('*') if f.is_file()}
changed={r for r,d in files.items() if p['files'].get(r)!=d}
assert changed=={'src/policies/default_policy.cpp','src/policies/transitive-schedule-6350.h'},changed
patch=''
for rel in sorted(changed):
    a,b=old/rel,source/rel
    patch+=''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],b.read_text().splitlines(True),
        fromfile='a/'+rel if a.exists() else '/dev/null',tofile='b/'+rel,n=0))
(c/'candidate.patch').write_text(patch)
(c/'manifest.json').write_text(json.dumps(dict(source=p['candidate'],files=files,binaries=binaries,
    parents_sha256=sha(c/'parents.json'),patch_sha256=sha(c/'candidate.patch'),release_exact_parent=True,
    logs={str(f.relative_to(c)):sha(f) for f in (c/'logs').iterdir()},
    tests='106Release105trace; actual shared C/C++ writer fixtures byte-identical. Full ordinary+ALL-census trace and partial-proof parent replay required before interpretation.',
    limitation='Read-only observer only. Reuses6350 adapter; no semantic/test-surface change and no production promotion.'),indent=2)+'\n')
print(json.dumps(binaries,indent=2))
