#!/usr/bin/env python3
"""Validate actual old/new repeated-pop observers and freeze one bounded run."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
controls = root / 'benchmarks/runs/20260908-5985-bve-repeat-heap-controls'
native = root / 'tools/solvers/variants/kissat-bve-repeat-heap-5985'
oldnative = root / 'tools/solvers/variants/kissat-bve-full-heap-5956'
tactix = root / '.claude/worktrees/runtime-bve-repeat-heap-5985'
build = root / 'build/pin-5985-bve-repeat-heap-trace'
assert '100% tests passed out of 97' in (controls / 'logs/tactix-ctest.log').read_text()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
spec = importlib.util.spec_from_file_location('repeat',Path(__file__).with_name('validate-bve-repeat-heap-5987.py'))
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v);v.fixtures()
cases={}
for name,source,all_pops,count in [('new-default',native,False,2),('new-all',native,True,3),('old-all',oldnative,True,2)]:
    exe=controls/(name+'-control');trace=controls/(name+'.tsv')
    assert not exe.exists() and not trace.exists()
    subprocess.run(['gcc','-DNDEBUG','-O2','-I'+str(source/'src'),str(Path(__file__).with_name('test-bve-repeat-native-5988.c')),str(source/'build/libkissat.a'),'-lm','-o',str(exe)],check=True)
    subprocess.run([exe,trace,str(int(all_pops))],check=True)
    with trace.open() as stream:images=v.parse(stream,271079,1)
    assert len(images)==count and all(rows==images[0] for rows in images)
    cases[name]=dict(snapshot_count=count,trace_sha256=sha(trace),binary_sha256=sha(exe))
(controls/'summary.json').write_text(json.dumps(dict(tactix_ctest='97/97 PASS',native_controls=cases,
    test_consideration='Old observer ignores all-pops and captures2 instead of3 frames;new default preserves2,all-pops records3. New Tactix fixed API fixture covers3 repeated pops. Strict parser rejects missing/ambiguous target size and bad repeated framing.'),indent=2)+'\n')
parent=root/'benchmarks/runs/20260908-5978-bve-earlier-heap'
c=root/'benchmarks/runs/20260908-5988-bve-repeat-heap'
m=json.loads((parent/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(parent/rel)==digest
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results','source'):(c/name).mkdir()
shutil.copy2(build/'tactix',c/'pins/candidate');shutil.copy2(native/'build/kissat',c/'pins/kissat')
runner=(parent/'run.py').read_text().replace('tactix-5978-','tactix-5988-')
anchor="env['SAT_BVE_FULL_HEAP_EXTERNAL'] = '462198'"
assert runner.count(anchor)==1
runner=runner.replace(anchor,anchor+"\nenv['SAT_BVE_FULL_HEAP_ALL_POPS'] = '1'")
compile(runner,str(c/'run.py'),'exec');(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)).replace('5978-bve-earlier-heap','5988-bve-repeat-heap'))
for arm,old,new,names in [('tactix',root/'.claude/worktrees/runtime-bve-full-heap-5956',tactix,['src/policies/default_policy.cpp','tests/test_bve_full_heap.cpp']),
                         ('kissat',oldnative,native,['src/eventtrace.c'])]:
    patch=''
    for name in names:
        r=subprocess.run(['diff','-U0','--label','a/'+name,'--label','b/'+name,str(old/name),str(new/name)],capture_output=True,text=True)
        assert r.returncode==1;patch+=r.stdout
        shutil.copy2(new/name,c/'source'/f'{arm}-{Path(name).name}')
    (c/(arm+'-observer.patch')).write_text(patch)
shutil.copy2(tactix/'tests/test_bve_full_heap_all.cpp',c/'source/test_bve_full_heap_all.cpp')
m.update(parent_campaign=str(parent.relative_to(root)),candidate_source=str(tactix.relative_to(root)),native_source=str(native.relative_to(root)),
    candidate_parent='.claude/worktrees/runtime-bve-full-heap-5956',native_parent=str(oldnative.relative_to(root)),
    hypothesis='Retired/failed variables can physically re-enter heaps. Capture EVERY matching pop462198 at271079,then select the unique8966-member state motivating60705 placement8930/8954.5978 first-occurrence state was62185 exactmembers,not this target.',
    validation='Complete ordinary300K traces,partial proof andBVE journals replay retained parent;all repeated images framed andvalidated,unique8966-member image required before interpretation.',
    observer_controls=dict(path=str((controls/'summary.json').relative_to(root)),sha256=sha(controls/'summary.json')),
    test_consideration='97/97 trace suite,actual old/new native default/repeated controls,strict multi-frame and unique-target parser controls;diagnostic-only no solver semantics change.',
    native_source_files={str(p.relative_to(native)):sha(p) for p in (native/'src').iterdir() if p.is_file()},
    native_parent_source_files={str(p.relative_to(oldnative)):sha(p) for p in (oldnative/'src').iterdir() if p.is_file()})
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c/'run.py',c/'array.sbatch',*(c/'pins').iterdir(),*(c/'source').iterdir(),*c.glob('*.patch')]}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
print(c)
