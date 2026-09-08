#!/usr/bin/env python3
"""Validate fixed native snapshots and freeze the complete-heap campaign."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
controls=root/'benchmarks/runs/20260908-5956-bve-full-heap-controls'
native=root/'tools/solvers/variants/kissat-bve-full-heap-5956'
tactix=root/'.claude/worktrees/runtime-bve-full-heap-5956'
build=root/'build/pin-5956-bve-full-heap-trace'
assert '100% tests passed out of 96' in (controls/'logs/tactix-ctest.log').read_text()
exe=controls/'native-control';trace=controls/'native-full.tsv'
assert not exe.exists() and not trace.exists()
cmd=['gcc','-DNDEBUG','-O2','-I'+str(native/'src'),str(Path(__file__).with_name('test-bve-full-heap-native-5958.c')),str(native/'build/libkissat.a'),'-lm','-o',str(exe)]
subprocess.run(cmd,check=True);subprocess.run([exe,trace],check=True)
lines=trace.read_text().splitlines()
assert len(lines)==13 and lines[0]=='# bve-full-heap\t1'
assert lines[1]=='# begin\t0\tinitial\t271079\t0\t4'
assert lines[7]=='# begin\t1\tbefore-pop\t271079\t1\t4'
assert lines[2:6]==lines[8:12]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
(controls/'summary.json').write_text(json.dumps(dict(tactix_ctest='96/96 PASS',native_control='actual construction/initial/pre-pop and repeat suppression PASS',
    hashes={str(p.relative_to(root)):sha(p) for p in [controls/'logs/tactix-ctest.log',exe,trace,tactix/'src/policies/default_policy.cpp',tactix/'tests/test_bve_full_heap.cpp',native/'src/eventtrace.c',native/'src/eliminate.c',build/'tactix',native/'build/kissat']}),indent=2)+'\n')
parent=root/'benchmarks/runs/20260908-5949-bve-heap-lifecycle'
c=root/'benchmarks/runs/20260908-5958-bve-full-heap'
m=json.loads((parent/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(parent/rel)==digest
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results','source'): (c/name).mkdir()
shutil.copy2(build/'tactix',c/'pins/candidate');shutil.copy2(native/'build/kissat',c/'pins/kissat')
runner=(parent/'run.py').read_text().replace('tactix-5949-','tactix-5958-')
start=runner.index("env[prefix + '_BVE_HEAP_TRACE']")
end=runner.index("assert not any(key.endswith('_EVENT_TRACE')",start)
runner=runner[:start]+'''env[prefix + '_BVE_FULL_HEAP_TRACE'] = str(c / 'traces' / (arm + '.full-heap.tsv'))
env['SAT_BVE_FULL_HEAP_CONFLICT'] = '271079'
env['SAT_BVE_FULL_HEAP_EXTERNAL'] = '122629'
'''+runner[end:]
compile(runner,str(c/'run.py'),'exec');(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)).replace('5949-bve-heap-lifecycle','5958-bve-full-heap'))
for arm,old,new,names in [('tactix',root/'.claude/worktrees/runtime-reduce-root-5900',tactix,['src/policies/default_policy.cpp']),
                          ('kissat',root/'tools/solvers/variants/kissat-bve-heap-lifecycle-5948',native,['src/eventtrace.c','src/eliminate.c'])]:
    patch=''
    for name in names:
        r=subprocess.run(['diff','-U0','--label','a/'+name,'--label','b/'+name,str(old/name),str(new/name)],capture_output=True,text=True)
        assert r.returncode==1;patch+=r.stdout
        shutil.copy2(new/name,c/'source'/f'{arm}-{Path(name).name}')
    (c/(arm+'-observer.patch')).write_text(patch)
shutil.copy2(tactix/'tests/test_bve_full_heap.cpp',c/'source/test_bve_full_heap.cpp')
m.update(parent_campaign=str(parent.relative_to(root)),candidate_source=str(tactix.relative_to(root)),native_source=str(native.relative_to(root)),
    candidate_parent='.claude/worktrees/runtime-reduce-root-5900',native_parent='tools/solvers/variants/kissat-bve-heap-lifecycle-5948',
    hypothesis='Initial full heap versus immediately before common pop122629 separates inherited initial topology from subsequent update drift and identifies both differing sift subtrees.',
    validation='Every ordinary300K trace andpartial proof matches cached5907/5771. Complete BVE candidates match5949. Exactly two full-heap snapshots, firstinitial andfirstbeforepop122629 at271079; full width12CPU/48000MiB,25min allocation.',
    test_consideration='96/96 trace suite including conflict filter, exact initial/pre-pop selection and repeated-call suppression. Actual native construction/control snapshots PASS. No solver behavior change; require complete replay before interpretation.',
    observer_controls=dict(path=str((controls/'summary.json').relative_to(root)),sha256=sha(controls/'summary.json')),
    native_source_files={str(p.relative_to(native)):sha(p) for p in (native/'src').iterdir() if p.is_file()},
    native_parent_source_files={str(p.relative_to(root/m['native_source'])):sha(p) for p in (root/m['native_source']/'src').iterdir() if p.is_file()},
    limitation='Observer-only5900 clone, not a new implementation candidate. No runtime/reference refresh. Existing campaigns andpins immutable; fuzz approval still denied.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c/'run.py',c/'array.sbatch',*(c/'pins').iterdir(),*(c/'source').iterdir(),*c.glob('*.patch')]}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
print(c)
