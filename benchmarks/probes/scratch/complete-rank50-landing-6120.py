#!/usr/bin/env python3
"""Preserve frozen unified-patch context bytes; finish the exact staged landing."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
root=Path(__file__).resolve().parents[3]
previous=root/'benchmarks/reports/2026-09-08-rank50-publication-landing.json'
target=root/'benchmarks/reports/2026-09-08-rank50-publication-landing-completion.json'
assert not target.exists()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads(previous.read_text())
owned=set(m['hashes'])|{str(previous.relative_to(root))}
for rel,digest in m['hashes'].items():assert sha(root/rel)==digest,rel
staged=set(subprocess.run(['git','diff','--cached','--name-only'],cwd=root,capture_output=True,text=True,check=True).stdout.splitlines())
assert staged==owned,(staged-owned,owned-staged)
p=subprocess.run(['git','diff','--cached','--check'],cwd=root,capture_output=True,text=True)
assert p.returncode==2 and not p.stderr
lines=p.stdout.splitlines();assert len(lines)%2==0
warnings=[]
allowed={
 'benchmarks/runs/20260908-6107-substitute-publication/cumulative.patch',
 'benchmarks/runs/20260908-6107-substitute-publication/repair.patch',
 'benchmarks/runs/20260908-6114-substitute-publication-BC-gate/pins/candidate.patch',
 'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50/pins/candidate-source.patch'}
for i in range(0,len(lines),2):
    match=re.fullmatch(r'(.+):(\d+): trailing whitespace\.',lines[i]);assert match,lines[i]
    rel,line=match.group(1),int(match.group(2))
    assert rel in allowed and lines[i+1]=='+ ',lines[i:i+2]
    assert (root/rel).read_text().splitlines()[line-1]==' ',(rel,line)
    warnings.append(dict(path=rel,line=line,content='single unified-diff context marker'))
warned={w['path'] for w in warnings}
assert warnings
subprocess.run(['git','diff','--cached','--check','--',*sorted(owned-warned)],cwd=root,check=True)
target.write_text(json.dumps(dict(parent_landing=dict(path=str(previous.relative_to(root)),sha256=sha(previous)),
    reason='The generic whitespace check reads a unified diff as source. Its blank context lines contain the required one-space context marker. Only these exact frozen patch lines are exempted; all other staged files pass unchanged.',
    warnings=warnings,patch_hashes={rel:sha(root/rel) for rel in sorted(warned)},
    cleanup='No frozen bytes changed or deleted;active campaigns untouched.',
    outcome='Resume exact6119stagedset and commit locally;no push.'),indent=2)+'\n')
new=[str(Path(__file__).relative_to(root)),str(target.relative_to(root))]
subprocess.run(['git','add','-f','--',*new],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check','--',*new],cwd=root,check=True)
subprocess.run(['git','commit','-m','Expand runtime target to Rank 1-50 and repair Substitute publication order'],cwd=root,check=True)
assert not subprocess.run(['git','status','--porcelain','--',*sorted(owned),*new],cwd=root,capture_output=True,text=True,check=True).stdout
print('PASS: coherent local commit;preserved exact unified-diff context;no push')
