#!/usr/bin/env python3
"""Freeze the independently validated inactive-pop policy repair."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5962-bve-active-pop'
source=root/'.claude/worktrees/runtime-bve-active-pop-5962'
parent=root/'.claude/worktrees/runtime-reduce-root-5900'
authority=root/'benchmarks/runs/20260908-5900-reduce-root/manifest.json'
assert not (c/'manifest.json').exists()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
a=json.loads(authority.read_text())
for rel,digest in a['files'].items():assert sha(parent/rel)==digest
builds={name:root/f'build/pin-{number}-bve-active-pop-{name}' for name,number in [('release',5962),('bc',5963),('trace',5964),('journal',5965)]}
assert sha(builds['release']/'tactix')==sha(builds['bc']/'tactix')
for name,count in [('release',97),('bc',97),('trace',96),('journal',97)]:
    assert f'100% tests passed out of {count}' in (c/f'logs/{name}-ctest-retry.log').read_text()
files=['src/policies/default_policy.cpp','tests/test_bve_active_pop.cpp']
changed=[str(p.relative_to(source)) for folder in ('src','include','tests') for p in (source/folder).rglob('*') if p.is_file() and (not (parent/p.relative_to(source)).exists() or sha(p)!=sha(parent/p.relative_to(source)))]
assert sorted(changed)==sorted(files),changed
def patch(base,names):
    return ''.join(''.join(difflib.unified_diff((base/name).read_text().splitlines(True) if (base/name).exists() else [],
        (source/name).read_text().splitlines(True),fromfile='a/'+name if (base/name).exists() else '/dev/null',tofile='b/'+name,n=0)) for name in names)
(c/'repair.patch').write_text(patch(parent,files))
(c/'cumulative.patch').write_text(patch(root,[str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))
for name in files:
    dest=c/'source'/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/name,dest)
v=json.loads((c/'final-proof-validation.json').read_text());assert v['checked_proofs']==54
for rel,digest in v['hashes'].items():assert sha(root/rel)==digest
m=dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),parent_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),production=a['production'],
    purpose='Skip root-assigned or retired BVE payloads at policy pop before the effort limit. Keep physical ghost reinsertion and heap pop unchanged;public BVE guard/instances/apply untouched. No new parameter,primitive,proof vocabulary or rank/input predicate.',
    test_consideration='97/97 Release twice,96/96 trace,97/97 journal;24 old/new cases,6 native cases,54 independently checked proofs. Two-sided root-satisfied large deletion reproduces old4/new3 retirements;public retry-independent instances andoriginal SAT models pass.',
    files={name:sha(source/name) for name in files},binaries={str((b/'tactix').relative_to(root)):sha(b/'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},validation={str((c/'final-proof-validation.json').relative_to(root)):sha(c/'final-proof-validation.json')},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()},
    limitation='Isolated/unfuzzed,not promoted. Fuzz transfer remains denied/no retry. Runtime effect unmeasured. Independent5900 successor,not regressing5920 composition or5956 diagnostic source.')
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for b in builds.values():p=b/'tactix';p.chmod(p.stat().st_mode&~0o222)
print('Frozen5962 inactive-pop policy repair')
