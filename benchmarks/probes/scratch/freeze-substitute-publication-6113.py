#!/usr/bin/env python3
"""Freeze the tested general policy repair independently of submitted sources."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6107-substitute-publication'
source=root/'.claude/worktrees/runtime-substitute-publication-6107'
parent=root/'.claude/worktrees/runtime-reduce-root-lock-6049'
authority=root/'benchmarks/runs/20260908-6049-reduce-root-lock/manifest.json'
assert not (c/'manifest.json').exists()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
a=json.loads(authority.read_text());b=json.loads((c/'build-summary.json').read_text())
for rel,digest in a['files'].items():assert sha(parent/rel)==digest,rel
for rel,digest in b['files'].items():assert sha(source/rel)==digest,rel
for rel,digest in b['binaries'].items():assert sha(root/rel)==digest,rel
for name in ('release','bc'):
    assert '100% tests passed out of 99' in (c/f'logs/6111-{name}-ctest.log').read_text()
files=['src/policies/default_policy.cpp','tests/test_substitute_publication.cpp']
changed=[str(p.relative_to(source)) for folder in ('src','include','tests') for p in (source/folder).rglob('*')
         if p.is_file() and (not (parent/p.relative_to(source)).exists() or sha(p)!=sha(parent/p.relative_to(source)))]
assert sorted(changed)==sorted(files),changed
def patch(base,names):
    return ''.join(''.join(difflib.unified_diff((base/name).read_text().splitlines(True) if (base/name).exists() else [],
        (source/name).read_text().splitlines(True),fromfile='a/'+name if (base/name).exists() else '/dev/null',tofile='b/'+name,n=3)) for name in names)
(c/'repair.patch').write_text(patch(parent,files))
(c/'cumulative.patch').write_text(patch(root,[str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))
for name in files:
    dest=c/'source'/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/name,dest)
m=dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),
    parent_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),production=a['production'],
    purpose='Publish each Substitute round root/retirement lifecycle before later rounds and watch reconnection. Policy-only; no public rule, guard, proof delta, budget/cap, score comparator or benchmark selector changed.',
    test_consideration='99/99 Release twice with identical independent pins. Eight actual policy cases cover initial/search,budgeted/unbudgeted,one/two-round caps;old6049 fails exact heap assertion. Existing Substitute/public API/reconstruction/proof/invariant and slot-budget tests retained. Full checked B28+C5 andRank1-50 follow;newsource fuzz still required.',
    files={name:sha(source/name) for name in files},binaries=b['binaries'],
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    validation={str((c/name).relative_to(root)):sha(c/name) for name in ('causal-replay.json','build-summary.json')},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()},
    limitation='Isolated/unfuzzed,not promoted. First expanded fixture missed preprocessing admission;6111 corrects fixture only and retains first failed logs. Runtime gain and real prefix closure unproven.')
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for rel in b['binaries']:
    p=root/rel;p.chmod(p.stat().st_mode&~0o222)
print('Frozen6107 general Substitute publication repair')
