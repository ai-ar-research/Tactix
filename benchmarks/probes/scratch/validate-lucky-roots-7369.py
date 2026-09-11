#!/usr/bin/env python3
"""Seal completed7366 builds after unsuppressed sanitizers outside ptrace."""
import json
import os
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
source=root/'.claude/worktrees/runtime-lucky-roots-7365'
old=root/'benchmarks/runs/20260911-7366-lucky-roots-build'
c=root/'benchmarks/runs/20260911-7369-lucky-roots-validation';assert not c.exists();(c/'logs').mkdir(parents=True)
files={str(p.relative_to(source)):sha(p) for folder in ('src','include','tests','examples') for p in sorted((source/folder).rglob('*')) if p.is_file()}
files['CMakeLists.txt']=sha(source/'CMakeLists.txt')
hashes={str((source/r).relative_to(root)):h for r,h in files.items()};binaries={}
def bind(p):hashes[str(p.relative_to(root))]=sha(p)
for kind,count in [('release',124),('bc',124),('trace',123)]:
    for stage in ('configure','build','ctest'):
        p=old/'logs'/f'{kind}-{stage}.json';r=json.loads(p.read_text());assert r['returncode']==0;bind(p)
        if stage=='ctest':assert f'100% tests passed out of {count}' in r['stdout'] and 'Skipped' not in r['stdout']
    p=root/f'build/pin-7366-lucky-roots-{kind}/tactix';bind(p);binaries[kind]={str(p.relative_to(root)):sha(p)}
assert list(binaries['release'].values())==list(binaries['bc'].values())
failed=old/'logs/sanitizers-0-lucky_roots.json';f=json.loads(failed.read_text());bind(failed)
assert f['returncode']!=0 and 'LeakSanitizer does not work under ptrace' in f['stderr']
for stage in ('configure','build'):
    p=old/'logs'/f'sanitizers-{stage}.json';r=json.loads(p.read_text());assert r['returncode']==0;bind(p)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
fixtures=('lucky_roots','lucky','root_fold','root_watermark','collect_connection','forward_unit','substitute_collapse','substitute_retry','subsume_material','compact_stamp','kernel','reconstruct','propagation_cursor','api_c','api_instances','bve')
san={}
for repeat in range(2):
    for name in fixtures:
        exe=root/'build/pin-7366-lucky-roots-sanitizers/tests'/('test_'+name)
        r=subprocess.run([str(exe)],cwd=c,env=env,text=True,capture_output=True)
        p=c/'logs'/f'sanitizers-{repeat}-{name}.json'
        with p.open('x') as out:json.dump(dict(command=[str(exe)],returncode=r.returncode,stdout=r.stdout,stderr=r.stderr),out,indent=2)
        bind(p);bind(exe);assert r.returncode==0,(name,r.stdout,r.stderr)
        san[str(exe.relative_to(root))]=sha(exe);print(repeat,name,'PASS',flush=True)
binaries['sanitizers']=san
for p in (Path(__file__),Path(__file__).with_name('build-lucky-roots-7366.py'),
          root/'benchmarks/runs/20260911-7365-lucky-roots-prepare/manifest.json',
          root/'benchmarks/runs/20260911-7364-lucky-root-parent/manifest.json',
          root/'benchmarks/reports/2026-09-11-root-lifecycle-analysis-7367.json'):bind(p)
for rel,digest in files.items():assert sha(source/rel)==digest
with (c/'manifest.json').open('x') as out:json.dump(dict(status='PASS',source=str(source.relative_to(root)),files=files,hashes=hashes,binaries=binaries,
    test_consideration='124 Release tests in each independent build,123 Trace,16 focused fixtures twice under unsuppressed ASAN/UBSAN/LSAN outside ptraced sandbox; actual-parent12-case negative regression.',
    preserved_failure='7366 full build helper failed only because LSAN cannot run under ptrace; original failure retained. Same pinned binaries rerun without ptrace, no suppression/rebuild.',
    limitation='Independent original-CNF proofs/models and broad BC remain required; no current-source fuzz, runtime-gain or promotion claim.'),out,indent=2)
print(c/'manifest.json')
