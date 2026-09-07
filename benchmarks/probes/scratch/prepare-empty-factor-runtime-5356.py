#!/usr/bin/env python3
"""Checked same-window current / cumulative / repaired / native witnesses."""
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5344-round-runtime'
new=root/'benchmarks/runs/20260907-5356-empty-factor-runtime'
new.mkdir(exist_ok=False)
for name in ('pins','logs','results','provisional','claims'):(new/name).mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
variants={'current':old/'pins/current','previous':old/'pins/bridge_round',
          'candidate':root/'build/pin-5356-empty-factor-release/tactix','kissat':old/'pins/kissat'}
arms={}
for name,binary in variants.items():
    shutil.copy2(binary,new/'pins'/name)
    arms[name]=dict(path='pins/'+name,sha256=sha(binary),source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',arguments=[])
    if name=='kissat':arms[name].update(source_commit='8af8e56f174b778aef3aa45af9f739b2a5f492c2',arguments=['-v'])
patch=''
for before in sorted((root/'src').rglob('*')):
    if not before.is_file():continue
    rel=before.relative_to(root);after=root/'.claude/worktrees/runtime-empty-factor-5354'/rel
    if before.read_bytes()==after.read_bytes():continue
    r=subprocess.run(['diff','-u','--label','a/'+str(rel),'--label','b/'+str(rel),before,after],capture_output=True,text=True)
    assert r.returncode==1;patch+=r.stdout
(new/'pins/candidate.patch').write_text(patch)
shutil.copy2(old/'pins/bridge_round.patch',new/'pins/previous.patch')
for name in ('candidate','previous'):
    arms[name].update(source_patch='pins/'+name+'.patch',source_patch_sha256=sha(new/'pins'/(name+'.patch')))
for name in ('checkmodel','drat-trim','modelcheck.py','runtime_task.py'):shutil.copy2(old/'pins'/name,new/'pins'/name)
with (old/'matrix.tsv').open(newline='') as f:
    reader=csv.DictReader(f,delimiter='\t');fields=reader.fieldnames
    rows=[r for r in reader if r['arm']=='current']
with (new/'matrix.tsv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=fields,delimiter='\t');w.writeheader()
    for i,row in enumerate(rows):
        for j,name in enumerate(variants):w.writerow(dict(row,task=4*i+j,arm=name))
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)).replace('--array=0-14','--array=0-11'))
m=json.loads((old/'manifest.json').read_text());m.pop('slurm_job_id',None)
m.update(arms=arms,parent_campaign=str(old.relative_to(root)),
    hypothesis='Empty Factor housekeeping restores BVE priority and moves first decision split from 95801 to 133243; measure downstream work and runtime against exact preceding cumulative and production pins.',
    comparison='Four proof-on independent Release arms on predeclared Ranks 17/21/34, one diagnostic wave.',
    decision='All 12 verdicts independently checked/clean; no promotion or general competitiveness claim from this subset. Cumulative B28+C5 and Claude-owned fuzz remain required.',
    candidate_validation='Isolated UNFUZZED 5354 patch, native positive / old negative / candidate positive fixed replay; 76/76 trace tests, fresh Release full tests captured separately.')
m['resource_contract']['array']='0-11'
m['pin_sha256']={str(p.relative_to(new)):sha(p) for p in (new/'pins').iterdir()}
for name in ('matrix.tsv','array.sbatch'):m['pin_sha256'][name]=sha(new/name)
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
