#!/usr/bin/env python3
"""Immutable optimized ablation pin with full125-test suite."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
source=root/'.claude/worktrees/runtime-corrected-legacy-7378'
pin=root/'build/pin-7379-corrected-legacy-release';c=root/'benchmarks/runs/20260911-7379-corrected-legacy-build'
assert not pin.exists() and not c.exists();(c/'logs').mkdir(parents=True)
files={str(p.relative_to(source)):sha(p) for folder in ('src','include','tests','examples') for p in sorted((source/folder).rglob('*')) if p.is_file()}
files['CMakeLists.txt']=sha(source/'CMakeLists.txt')
hashes={str((source/rel).relative_to(root)):digest for rel,digest in files.items()}
for name,command in [('configure',['cmake','-S',source,'-B',pin,'-DCMAKE_BUILD_TYPE=Release','-DTACTIX_ENABLE_EVENT_TRACE=OFF','-DTACTIX_ENABLE_RULE_JOURNAL=OFF','-DTACTIX_ENABLE_PROOF_SHADOW=OFF','-DTACTIX_ENABLE_CONFLICT_STOP=OFF','-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc','-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++']),('build',['cmake','--build',pin,'-j4']),('ctest',['ctest','--test-dir',pin,'--output-on-failure','-j1'])]:
    command=list(map(str,command));r=subprocess.run(command,cwd=c,text=True,capture_output=True)
    p=c/'logs'/(name+'.json')
    with p.open('x') as out:json.dump(dict(command=command,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr),out,indent=2)
    hashes[str(p.relative_to(root))]=sha(p);assert r.returncode==0,(name,r.stdout[-3000:],r.stderr[-3000:])
    if name=='ctest':assert '100% tests passed out of 125' in r.stdout and 'Skipped' not in r.stdout
    print(name,'PASS',flush=True)
for rel,digest in files.items():assert sha(source/rel)==digest
for p in [Path(__file__),root/'benchmarks/runs/20260911-7378-corrected-legacy-prepare/manifest.json',pin/'tactix']:hashes[str(p.relative_to(root))]=sha(p)
with (c/'manifest.json').open('x') as out:json.dump(dict(status='PASS',source=str(source.relative_to(root)),files=files,hashes=hashes,binaries={str((pin/'tactix').relative_to(root)):sha(pin/'tactix')},test_consideration='Configuration ablation at one existing call, no new rule or API. Full125 tests preserve both explicit folding and legacy compaction fixtures plus Lucky lifecycle and guidance reattachment tests. No performance claim.'),out,indent=2)
print(c/'manifest.json')
