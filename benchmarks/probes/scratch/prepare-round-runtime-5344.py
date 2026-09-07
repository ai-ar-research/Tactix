#!/usr/bin/env python3
"""Checked crossed ablation of per-round retirement order on three witnesses."""
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5329-bridge-unit-small'
new = root/'benchmarks/runs/20260907-5344-round-runtime'
new.mkdir(exist_ok=False)
for name in ('pins','logs','results','provisional','claims'):
    (new/name).mkdir()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
variants = [
    ('current',old/'pins/shipped',None),
    ('round_only',root/'build/pin-5344-round-only/tactix',root/'.claude/worktrees/runtime-round-only-5344'),
    ('bridge',old/'pins/candidate',None),
    ('bridge_round',root/'build/pin-5344-round-release/tactix',root/'.claude/worktrees/runtime-round-release-5344'),
    ('kissat',root/'benchmarks/runs/20260907-5314-pose-r2-ranks1-40/pins/kissat',None)]
arms = {}
for name,binary,source in variants:
    shutil.copy2(binary,new/'pins'/name)
    arms[name] = dict(path='pins/'+name,sha256=sha(binary),source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',arguments=[])
    patch = ''
    if source:
        for before in sorted((root/'src').rglob('*')):
            if not before.is_file():
                continue
            rel = before.relative_to(root)
            after = source/rel
            if before.read_bytes() == after.read_bytes():
                continue
            result = subprocess.run(['diff','-u','--label','a/'+str(rel),'--label','b/'+str(rel),before,after],text=True,capture_output=True)
            assert result.returncode == 1
            patch += result.stdout
    elif name == 'bridge':
        patch = (old/'pins/candidate.patch').read_text()
    elif name == 'kissat':
        arms[name].update(source_commit='8af8e56f174b778aef3aa45af9f739b2a5f492c2',arguments=['-v'])
    if patch:
        p = new/'pins'/(name+'.patch')
        p.write_text(patch)
        arms[name].update(source_patch=str(p.relative_to(new)),source_patch_sha256=sha(p))
for name in ('checkmodel','drat-trim','modelcheck.py','runtime_task.py'):
    shutil.copy2(old/'pins'/name,new/'pins'/name)
with (old/'matrix.tsv').open(newline='') as f:
    reader = csv.DictReader(f,delimiter='\t')
    fields = reader.fieldnames
    rows = [row for row in reader if row['arm'] == 'shipped']
with (new/'matrix.tsv').open('w',newline='') as f:
    writer = csv.DictWriter(f,fieldnames=fields,delimiter='\t')
    writer.writeheader()
    for i,row in enumerate(rows):
        for j,(name,_,_) in enumerate(variants):
            writer.writerow(dict(row,task=i*len(variants)+j,arm=name))
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)).replace('--array=0-5','--array=0-14'))
m = json.loads((old/'manifest.json').read_text())
m.pop('slurm_job_id',None)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),arms=arms,
    hypothesis='Removing the obsolete cross-round retirement sort closes the Rank-34 warm-up trace. Measure whether it repairs downstream work alone and on the cumulative core/sequential/pose stack, retaining Rank-17/21 witnesses.',
    comparison='Five immutable general-purpose Release arms; three instances, instance-major full-width array, proof-on all arms.',
    decision='15 valid models and clean scratch required. One diagnostic wave selects further validation; no promotion or global competitiveness claim.',
    parent_campaign=str(old.relative_to(root)),
    candidate_validation='5343 exact policy ablation: all 1488 inheap removals and 13292 warm-up decision/implication records match native reference. Fresh Release candidates UNFUZZED; full CTests recorded separately.')
m['resource_contract']['array'] = '0-14'
m['pin_sha256'] = {str(p.relative_to(new)):sha(p) for p in (new/'pins').iterdir()}
for name in ('matrix.tsv','array.sbatch'):
    m['pin_sha256'][name] = sha(new/name)
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
