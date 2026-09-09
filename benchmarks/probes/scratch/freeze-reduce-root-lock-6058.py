#!/usr/bin/env python3
"""Freeze the isolated root-reason retention repair and all exact build inputs."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6049-reduce-root-lock';assert not (c/'manifest.json').exists()
source=root/'.claude/worktrees/runtime-reduce-root-lock-6049'
parent=root/'.claude/worktrees/runtime-bve-active-pop-5962'
authority=root/'benchmarks/runs/20260908-5962-bve-active-pop/manifest.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
a=json.loads(authority.read_text())
for rel,digest in a['files'].items():assert sha(parent/rel)==digest,rel
builds={name:root/f'build/pin-{number}-reduce-root-lock-{name}' for name,number in [('release',6049),('bc',6050),('trace',6051),('journal',6052)]}
assert sha(builds['release']/'tactix')==sha(builds['bc']/'tactix')
for name,count in [('release',98),('bc',98),('trace',97),('journal',98)]:
    assert f'100% tests passed out of {count}' in (c/f'logs/{name}-ctest.log').read_text()
files=['src/rules/reduce/reduce.cpp','tests/test_reduce_root_lock.cpp']
changed=[str(p.relative_to(source)) for folder in ('src','include','tests') for p in (source/folder).rglob('*') if p.is_file() and (not (parent/p.relative_to(source)).exists() or sha(p)!=sha(parent/p.relative_to(source)))]
assert sorted(changed)==sorted(files),changed
def patch(base,names):
    return ''.join(''.join(difflib.unified_diff((base/name).read_text().splitlines(True) if (base/name).exists() else [],
        (source/name).read_text().splitlines(True),fromfile='a/'+name if (base/name).exists() else '/dev/null',tofile='b/'+name,n=0)) for name in names)
(c/'repair.patch').write_text(patch(parent,files))
(c/'cumulative.patch').write_text(patch(root,[str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))
for name in files:
    dest=c/'source'/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/name,dest)
v=json.loads((c/'complete-proof-validation.json').read_text());assert v['checked_proofs']==62
for rel,digest in v['hashes'].items():assert sha(root/rel)==digest,rel
m=dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),parent_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),production=a['production'],
    purpose='Exclude root assignments from transient Reduce reason locks, matching native flushed trail and CaDiCaL active-only reasons. Preserve pending/non-root locks, pure public enumeration, stale payload revalidation, proof units and GC retirement. No parameter, primitive, cutoff or benchmark-specific logic.',
    test_consideration='98/98 Release twice,97/97 trace,98/98 journal.16 root/SAT/UNSAT/proof-mode cases and2 protected/stale reason cases;actual native caller3 cases;62 independently checked DRAT/LRAT proofs.6055 aggregation-only path failure closed6057, no rejected oracle.',
    files={name:sha(source/name) for name in files},binaries={str((b/'tactix').relative_to(root)):sha(b/'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},validation={str((c/'complete-proof-validation.json').relative_to(root)):sha(c/'complete-proof-validation.json')},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()},
    limitation='Isolated/unfuzzed,not promoted. Fuzz transfer remains denied/no retry. Runtime not yet measured.6051 retains old diagnostic reason annotations;use corrected6054 diagnostic clone for full Reduce census comparisons.')
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for b in builds.values():p=b/'tactix';p.chmod(p.stat().st_mode&~0o222)
print('Frozen6049 general root-reason locking repair')
