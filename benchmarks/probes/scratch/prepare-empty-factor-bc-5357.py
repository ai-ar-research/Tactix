#!/usr/bin/env python3
"""Predeclared complete B28+C5 gate, separate identical candidate B/C builds."""
import csv
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5315-pose-r2-BC-gate'
runtime=root/'benchmarks/runs/20260907-5356-empty-factor-runtime'
new=root/'benchmarks/runs/20260907-5357-empty-factor-BC-gate'
new.mkdir(exist_ok=False)
for name in ('pins','logs','results','provisional','claims'):(new/name).mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
builds={'B':root/'build/pin-5356-empty-factor-release/tactix','C':root/'build/pin-5357-empty-factor-C/tactix'}
assert sha(builds['B'])==sha(builds['C'])
arms={}
for tier in ('B','C'):
    for name,binary in (('current',runtime/'pins/current'),('candidate',builds[tier])):
        arm=name+'_'+tier;shutil.copy2(binary,new/'pins'/arm)
        arms[arm]=dict(path='pins/'+arm,sha256=sha(binary),arguments=[],source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34')
        if name=='candidate':arms[arm].update(source_patch='pins/candidate.patch',source_patch_sha256=sha(runtime/'pins/candidate.patch'),source_build=str(binary.parent.relative_to(root)))
for name in ('checkmodel','drat-trim','modelcheck.py','runtime_task.py','candidate.patch'):
    shutil.copy2(runtime/'pins'/name,new/'pins'/name)
with (old/'matrix.tsv').open(newline='') as f:
    r=csv.DictReader(f,delimiter='\t');fields=r.fieldnames;rows=list(r)
assert len(rows)==66
with (new/'matrix.tsv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=fields,delimiter='\t');w.writeheader()
    for r in rows:w.writerow(dict(r,arm=('current' if r['arm']=='shipped' else 'candidate')+'_'+r['tier']))
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)).replace('5315-pose-r2-BC-gate','5357-empty-factor-BC-gate'))
m=json.loads((old/'manifest.json').read_text())
for k in ('slurm_job_id','fuzz_authority','candidate_patch','candidate_patch_sha256'):m.pop(k,None)
m.update(arms=arms,purpose='Complete B28+C5 regression gate on isolated cumulative empty Factor repair versus current production.',
    predecessor=str(old.relative_to(root)),
    hypothesis='Reference-complete empty Factor housekeeping can be retained on the cumulative repair stack without an unexplained checked solve loss; quantify aggregate and per-instance runtime regression.',
    validation='66 independently checked/clean results required; same frozen B28+C5 membership and 1200-second inherited limit. UNFUZZED investigation material: no promotion without Claude-owned consolidation fuzz.',
    build_identity='B and C use separate fresh Release build directories from the same immutable source copy; binary SHA-256 values match. All four arms have distinct read-only campaign pins.')
m['pin_sha256']={str(p.relative_to(new)):sha(p) for p in (new/'pins').iterdir()}
for name in ('matrix.tsv','array.sbatch'):m['pin_sha256'][name]=sha(new/name)
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
