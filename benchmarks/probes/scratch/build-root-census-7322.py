#!/usr/bin/env python3
"""Build OFF-identical control and isolated root census with full suites."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
import os
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260911-7322-root-census-build'
assert not c.exists(),'preserve successful/failed builds'
(c/'logs').mkdir(parents=True)
source=root/'.claude/worktrees/runtime-root-census-7322'
p=root/'benchmarks/runs/20260911-7302-clause-realloc-build/manifest.json'
parent=json.loads(p.read_text()); hashes={str(p.relative_to(root)):sha(p)}
files={str(p.relative_to(source)):sha(p) for name in ('src','include','tests') for p in (source/name).rglob('*') if p.is_file()}
files['CMakeLists.txt']=sha(source/'CMakeLists.txt')
changed={rel for rel,d in files.items() if parent['files'].get(rel)!=d}
assert changed=={'CMakeLists.txt','src/kernel/occ.hpp','src/kernel/connected_occurrence.hpp',
 'src/diagnostics/root_census.hpp','src/diagnostics/root_census.cpp','src/policies/default_policy.cpp','tests/test_root_census.cpp'},changed
patch=''
for rel,d in parent['files'].items(): assert sha(root/parent['source']/rel)==d,rel
for rel,d in files.items():
 hashes[str((source/rel).relative_to(root))]=d
 if rel in changed:
  old=root/parent['source']/rel
  patch+=''.join(difflib.unified_diff(old.read_text().splitlines(True) if old.exists() else [],
   (source/rel).read_text().splitlines(True),fromfile='a/'+rel if old.exists() else '/dev/null',tofile='b/'+rel))
with (c/'incremental.patch').open('x') as f:f.write(patch)
commands={}
def run(label,command,env=None):
 p=subprocess.run(list(map(str,command)),cwd=c,text=True,capture_output=True,env=env)
 with (c/'logs'/f'{label}.log').open('x') as f:f.write(p.stdout+p.stderr)
 commands[label]=dict(command=list(map(str,command)),exit_code=p.returncode)
 assert p.returncode==0,(label,p.stdout[-2000:],p.stderr[-2000:])
 print(label,'PASS',flush=True)
 return p.stdout
def build(kind):
 out=root/f'build/pin-7322-root-census-{kind}'
 assert not out.exists()
 run(kind+'-configure',['cmake','-S',source,'-B',out,'-DCMAKE_BUILD_TYPE=Release',
  '-DTACTIX_ENABLE_ROOT_CENSUS='+('ON' if kind=='observer' else 'OFF'),
  '-DTACTIX_ENABLE_EVENT_TRACE=OFF','-DTACTIX_ENABLE_RULE_JOURNAL=OFF',
  '-DTACTIX_ENABLE_PROOF_SHADOW=OFF','-DTACTIX_ENABLE_CONFLICT_STOP=OFF',
  '-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc',
  '-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++'])
 run(kind+'-build',['cmake','--build',out,'-j2'])
 run(kind+'-ctest',['ctest','--test-dir',out,'--output-on-failure','-j1'])
 binary=out/'tactix';digest=sha(binary)
 if kind=='control':assert digest==parent['binaries']['build/pin-7302-clause-realloc-release/tactix']
 assert (b'TACTIX_ROOT_CENSUS' in binary.read_bytes())==(kind=='observer')
 if kind=='observer':
  env=dict(os.environ);trace=c/'fixture.tsv';env['TACTIX_ROOT_CENSUS']=str(trace)
  run('observer-emission',[out/'tests/test_root_census'],env)
  rows=trace.read_text().splitlines()
  assert len(rows)==2 and 'stage=fixture' in rows[0] and 'stage=disconnect' in rows[1]
 return kind,dict(path=str(binary.relative_to(root)),sha256=digest,observer=kind=='observer')
with ThreadPoolExecutor(max_workers=2) as pool:binaries=dict(pool.map(build,('control','observer')))
for p in c.rglob('*'):
 if p.is_file():hashes[str(p.relative_to(root))]=sha(p)
hashes[str(Path(__file__).relative_to(root))]=sha(Path(__file__))
prep=Path(__file__).with_name('prepare-root-census-7322.py');hashes[str(prep.relative_to(root))]=sha(prep)
for rel,d in hashes.items():assert sha(root/rel)==d,rel
with (c/'manifest.json').open('x') as f:
 json.dump(dict(status='PASS',source=str(source.relative_to(root)),files=files,hashes=hashes,
  changed=sorted(changed),binaries=binaries,commands=commands,
  hypothesis='Root-fixed full-sized array retention and dense storage/lifetime, rather than longer total runtime, explain substantial Rank25 memory excess. Measure populations and allocator live/free totals without semantic or budget edits.',
  gate='OFF control byte-identical7302; both full suites and root/dense/emission/suppression tests. Real observer must reproduce exact own7302 work/certificate and ordinary peak2484880KiB before peak attribution. Diagnostic times never scored.',
  limitation='Observer may perturb allocation history; no new Kernel/Stats fields, rule/proof changes or source promotion. Pending independent checks do not block provisional investigation.'),f,indent=2)
print(c/'manifest.json')
