#!/usr/bin/env python3
"""Checked four-arm witness screen and complete B28+C5 regression gate."""
import csv
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5356-empty-factor-runtime'
runtime=root/'benchmarks/runs/20260907-5371-congruence-prefix-runtime'
gate=root/'benchmarks/runs/20260907-5372-congruence-prefix-BC-gate'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
builds={'B':root/'build/pin-5371-congruence-prefix-release/tactix',
        'C':root/'build/pin-5372-congruence-prefix-bc/tactix'}
assert sha(builds['B'])==sha(builds['C'])
def init(c):
 c.mkdir(exist_ok=False)
 for name in ('pins','logs','results','provisional','claims'): (c/name).mkdir()
def matrix(c,rows,fields):
 with (c/'matrix.tsv').open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields,delimiter='\t');w.writeheader();w.writerows(rows)
def finish(c,m):
 m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in (c/'pins').iterdir()}
 for name in ('matrix.tsv','array.sbatch'): m['pin_sha256'][name]=sha(c/name)
 (c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
 for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
 print(c)
init(runtime)
patch=''
for before in sorted((root/'src').rglob('*')):
 if not before.is_file(): continue
 rel=before.relative_to(root);after=root/'.claude/worktrees/runtime-congruence-prefix-5369'/rel
 patch+=''.join(difflib.unified_diff(before.read_text().splitlines(True),after.read_text().splitlines(True),fromfile='a/'+str(rel),tofile='b/'+str(rel)))
(runtime/'pins/candidate.patch').write_text(patch)
shutil.copy2(old/'pins/candidate.patch',runtime/'pins/previous.patch')
variants={'current':old/'pins/current','previous':old/'pins/candidate','candidate':builds['B'],'kissat':old/'pins/kissat'}
arms={}
for name,binary in variants.items():
 shutil.copy2(binary,runtime/'pins'/name)
 arms[name]=dict(path='pins/'+name,sha256=sha(binary),source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',arguments=[])
 if name=='kissat': arms[name].update(source_commit='8af8e56f174b778aef3aa45af9f739b2a5f492c2',arguments=['-v'])
 if name in ('previous','candidate'): arms[name].update(source_patch='pins/'+name+'.patch',source_patch_sha256=sha(runtime/'pins'/(name+'.patch')))
for name in ('checkmodel','drat-trim','modelcheck.py','runtime_task.py'): shutil.copy2(old/'pins'/name,runtime/'pins'/name)
shutil.copy2(old/'matrix.tsv',runtime/'matrix.tsv')
(runtime/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(runtime)))
m=json.loads((old/'manifest.json').read_text());m.pop('slurm_job_id',None)
m.update(arms=arms,parent_campaign=str(old.relative_to(root)),
 hypothesis='Congruence prefix cleanup closes the old trajectory frontier; price it against exact preceding cumulative candidate and current production.',
 comparison='Ranks 17/21/34, four independent proof-on arms in one diagnostic wave. No decision-grade timing claim.',
 decision='All 12 verdicts must be independently checked/clean. General improvement requires the full runtime gate; no promotion without Claude-owned consolidation fuzz.',
 candidate_validation='5369 native-positive/old-negative/repaired-positive fixture; 76 trace CTests. 5370 all 475997 decisions agree through 200000 conflicts. Fresh Release full tests retained separately.')
finish(runtime,m)

init(gate)
oldgate=root/'benchmarks/runs/20260907-5357-empty-factor-BC-gate'
arms={}
for tier in ('B','C'):
 for name,binary in [('current',runtime/'pins/current'),('candidate',builds[tier])]:
  arm=name+'_'+tier;shutil.copy2(binary,gate/'pins'/arm)
  arms[arm]=dict(path='pins/'+arm,sha256=sha(binary),arguments=[],source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34')
  if name=='candidate': arms[arm].update(source_patch='pins/candidate.patch',source_patch_sha256=sha(runtime/'pins/candidate.patch'),source_build=str(binary.parent.relative_to(root)))
for name in ('checkmodel','drat-trim','modelcheck.py','runtime_task.py','candidate.patch'): shutil.copy2(runtime/'pins'/name,gate/'pins'/name)
shutil.copy2(oldgate/'matrix.tsv',gate/'matrix.tsv')
(gate/'array.sbatch').write_text((oldgate/'array.sbatch').read_text().replace(str(oldgate),str(gate)).replace('5357-empty-factor-BC-gate','5372-congruence-prefix-BC-gate'))
m=json.loads((oldgate/'manifest.json').read_text());m.pop('slurm_job_id',None)
m.update(arms=arms,predecessor=str(oldgate.relative_to(root)),
 purpose='Complete checked B28+C5 gate for isolated cumulative Congruence prefix repair versus current production.',
 hypothesis='The native prefix cleanup preserves all checked solves; quantify aggregate/per-instance regressions.',
 validation='66 independently checked/clean final records required. Same frozen B28+C5 and 1200-second inherited solver limit. UNFUZZED and unpromoted.',
 build_identity='Two fresh independent Release builds from immutable 5369 source, identical hashes; four distinct read-only campaign pins.')
finish(gate,m)
