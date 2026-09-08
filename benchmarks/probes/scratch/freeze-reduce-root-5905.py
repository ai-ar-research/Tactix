#!/usr/bin/env python3
"""Seal independent Reduce root-guidance cleanup,not the regressing5870 branch."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5900-reduce-root'
source=root/'.claude/worktrees/runtime-reduce-root-5900'
parent=root/'.claude/worktrees/runtime-forward-caller-5849'
authority=root/'benchmarks/runs/20260908-5849-forward-caller/manifest.json'
assert not (c/'manifest.json').exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
a=json.loads(authority.read_text())
for rel,digest in a['files'].items(): assert sha(parent/rel)==digest
builds={name:root/f'build/pin-{number}-reduce-root-{name}' for name,number in [('release',5900),('bc',5901),('trace',5902),('journal',5903)]}
assert sha(builds['release']/'tactix')==sha(builds['bc']/'tactix')
for name,count in [('release',96),('bc',96),('trace',95),('journal',96)]:
    assert f'100% tests passed out of {count}' in (c/f'logs/{name}-ctest-final.log').read_text()
files=['src/policies/default_policy.cpp','src/policies/default_policy.hpp','tests/test_reduce_root_guidance.cpp']
changed=[str(p.relative_to(source)) for folder in ('src','include','tests') for p in (source/folder).rglob('*') if p.is_file() and (not (parent/p.relative_to(source)).exists() or sha(p)!=sha(parent/p.relative_to(source)))]
assert sorted(changed)==sorted(files),changed
def patch(base,names):
    return ''.join(''.join(difflib.unified_diff((base/name).read_text().splitlines(True) if (base/name).exists() else [],
        (source/name).read_text().splitlines(True),fromfile='a/'+name if (base/name).exists() else '/dev/null',tofile='b/'+name)) for name in names)
(c/'repair.patch').write_text(patch(parent,files))
(c/'cumulative.patch').write_text(patch(root,[str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))
for name in files:
    dest=c/'source'/name; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source/name,dest)
validation=c/'final-proof-validation.json'
v=json.loads(validation.read_text())
assert v['checked_proofs']==46
for rel,digest in v['hashes'].items(): assert sha(root/rel)==digest
evidence=[validation,root/'benchmarks/runs/20260908-5891-rank32-reorder-frontier/choice-validation.json']
m=dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),
    parent_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),production=a['production'],
    purpose='Consume newly derived root identities after Reduce preparation into DefaultPolicy queue/heap. Derive suffix start from exact new-root count after trail truncation. Public Reduce stays policy-neutral;no guard,primitive,budget,option or proof-vocabulary change. Independent5849 successor,not composed with regressing5870.',
    test_consideration='96/96 Release twice,95/95 trace,96/96 journal. New retained-root chain/truncated suffix,public Reduce neutrality,idempotence,no-root no-op,original SAT model andsix refutation cases. Actual old/new/native root-guidance controls.46 independently checked proofs including active-journal inherited cases.',
    files={name:sha(source/name) for name in files},binaries={str((b/'tactix').relative_to(root)):sha(b/'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},validation={str(p.relative_to(root)):sha(p) for p in evidence},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()},
    limitation='Isolated/unfuzzed,not promoted. Current-source fuzz remains blocked by previously denied external-transfer approval,no retry. Runtime and causal gates pending.5870 remains separate regressing investigation material.')
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for b in builds.values():
    p=b/'tactix'; p.chmod(p.stat().st_mode & ~0o222)
print('Frozen5900 independent Reduce root-guidance repair')
