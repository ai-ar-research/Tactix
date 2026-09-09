#!/usr/bin/env python3
"""Build source-bound diagnostic adapter and require ordinary Release identity."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6327-collapse-schedule-build'
assert not (c/'summary.json').exists()
p=json.loads((c/'parent.json').read_text());source=root/p['source'];parent=root/p['parent']
assert sha(root/p['authority']['path'])==p['authority']['sha256']
files={rel:sha(source/rel) for rel in p['files']}
for rel,digest in p['files'].items():assert sha(parent/rel)==digest,rel
changed=[rel for rel in files if files[rel]!=p['files'][rel]]
assert set(changed)=={'src/diagnostics/event_trace.cpp','tests/test_event_trace.cpp'}
def run(name,command):
    log=c/'logs'/(name+'.log')
    with log.open('x') as out:r=subprocess.run(command,stdout=out,stderr=subprocess.STDOUT)
    assert r.returncode==0,(name,log.read_text()[-5000:])
    print(name,'PASS',flush=True)
def build(kind,enabled):
    b=root/('build/pin-6327-collapse-schedule-'+kind);assert not b.exists()
    run(kind+'-configure',['cmake','-S',str(source),'-B',str(b),'-DCMAKE_BUILD_TYPE=Release',
        '-DTACTIX_ENABLE_CONFLICT_STOP=OFF','-DTACTIX_ENABLE_EVENT_TRACE='+enabled])
    run(kind+'-build',['cmake','--build',str(b),'-j6'])
    run(kind+'-ctest',['ctest','--test-dir',str(b),'--output-on-failure','-j1'])
    count=105 if kind=='release' else 104
    assert f'100% tests passed out of {count}' in (c/'logs'/(kind+'-ctest.log')).read_text()
    return str((b/'tactix').relative_to(root)),sha(b/'tactix')
with ThreadPoolExecutor(max_workers=2) as pool:
    jobs=[pool.submit(build,'release','OFF'),pool.submit(build,'trace','ON')]
    binaries=dict(f.result() for f in jobs)
assert binaries['build/pin-6327-collapse-schedule-release/tactix']=='33a94aec8df9a3be4f2a902e13d59dcc1527a412302f646140bab2ec5c4193e4'
for rel,digest in files.items():assert sha(source/rel)==digest
for rel in changed:
    patch=''.join(difflib.unified_diff((parent/rel).read_text().splitlines(True),(source/rel).read_text().splitlines(True),fromfile='a/'+rel,tofile='b/'+rel))
    (c/(Path(rel).name+'.patch')).write_text(patch)
(c/'summary.json').write_text(json.dumps(dict(source=p['source'],files=files,binaries=binaries,
    parent_sha256=sha(c/'parent.json'),changes=changed,release_exact_parent=True,
    logs={str(q.relative_to(c)):sha(q) for q in (c/'logs').iterdir()},
    tests='105Release/104trace including actual exit/continue/conflict/ordinal/no-overwrite/state-preservation fixtures. Same observer change already tested against native6206; real current6305 full replay remains required.',
    limitation='Diagnostic-only. No rule/policy/Release behavior change, performance claim or promotion.'),indent=2)+'\n')
print(json.dumps(binaries,indent=2))
