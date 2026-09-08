#!/usr/bin/env python3
"""Pin phase-entry causal comparison and unchanged full-view/regression gates."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260907-5430-inprocess-root'
assert (authority / 'manifest.json').exists()
builds = {t:root/p/'tactix' for t,p in [('B','build/pin-5430-inprocess-root-release'),('C','build/pin-5432-inprocess-root-bc')]}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(builds['B']) == sha(builds['C'])
def init(c, directories):
    c.mkdir(exist_ok=False)
    for d in directories: (c/d).mkdir()
def finish(c,m,key):
    m.pop('slurm_job_id',None)
    m['created_at'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    m['candidate_authority'] = str(authority.relative_to(root))
    entries = [*(c/'pins').iterdir(), c/'array.sbatch']
    for n in ('run.py','matrix.tsv'):
        if (c/n).exists(): entries.append(c/n)
    m[key] = {str(p.relative_to(c)):sha(p) for p in entries}
    (c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)
old = runs/'20260907-5424-rank35-rephase-prefix'
c = runs/'20260907-5433-rank35-inprocess-prefix'
init(c,('pins','logs','traces','results'))
shutil.copy2(root/'build/pin-5431-inprocess-root-trace/tactix',c/'pins/candidate')
shutil.copy2(old/'pins/kissat',c/'pins/kissat')
(c/'run.py').write_text((old/'run.py').read_text().replace("'tactix-5424-'","'tactix-5433-'"))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
m = json.loads((old/'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),candidate_source='.claude/worktrees/runtime-inprocess-root-5430',
    hypothesis='Restore Probe/Eliminate root entry and compare the complete Rank-35 prefix at 100000 conflicts against the Rephase-only witness and native reference.',
    validation='Two bounded stops with pin/trace identity and scratch cleanup. Compare common and full prefix plus first window split; a changed prefix is evidence requiring interpretation, not automatically a failed solver or successful repair.')
finish(c,m,'hashes')
old = runs/'20260907-5423-rephase-root-ranks1-40'
c = runs/'20260907-5434-inprocess-root-ranks1-40'
init(c,('pins','logs','results','provisional','claims'))
for name in ('kissat','drat-trim','checkmodel','modelcheck.py','runtime_task.py'): shutil.copy2(old/'pins'/name,c/'pins'/name)
shutil.copy2(builds['B'],c/'pins/tactix')
shutil.copy2(authority/'cumulative.patch',c/'pins/candidate-source.patch')
shutil.copy2(old/'matrix.tsv',c/'matrix.tsv')
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
m = json.loads((old/'manifest.json').read_text())
m.pop('causal_confirmation',None)
m.update(parent_campaign=str(old.relative_to(root)),
    purpose='Full unchanged Rank 1–40 comparison for Probe/Eliminate root closure on the Rephase stack; metadata ablation 5426 is not included.',
    hypothesis='Close a missing general root-fixpoint and tick-accounting boundary before simplifier rule admission. Measure all work/runtime/memory outcomes without selecting favorable trajectories.',
    candidate_validation='Old/new actual scheduler fixtures; trace 77/77 and independent matching Release builds 78/78 each; UNFUZZED.')
m['arms']['tactix'].update(sha256=sha(builds['B']),source_build=str(builds['B'].parent.relative_to(root)),source_patch_sha256=sha(authority/'cumulative.patch'))
finish(c,m,'pin_sha256')
old = runs/'20260907-5422-rephase-root-BC-gate'
c = runs/'20260907-5435-inprocess-root-BC-gate'
init(c,('pins','logs','results','provisional','claims'))
m = json.loads((old/'manifest.json').read_text())
for tier in ('B','C'):
    for name in ('current','candidate'):
        arm = name+'_'+tier
        binary = builds[tier] if name=='candidate' else old/'pins'/arm
        shutil.copy2(binary,c/'pins'/arm)
        m['arms'][arm]['sha256'] = sha(binary)
        if name=='candidate': m['arms'][arm].update(source_build=str(binary.parent.relative_to(root)),source_patch_sha256=sha(authority/'cumulative.patch'))
for name in ('checkmodel','drat-trim','modelcheck.py','runtime_task.py'): shutil.copy2(old/'pins'/name,c/'pins'/name)
shutil.copy2(authority/'cumulative.patch',c/'pins/candidate.patch')
shutil.copy2(old/'matrix.tsv',c/'matrix.tsv')
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('5422-rephase-root-BC','5435-inprocess-root-BC'))
m.update(predecessor=str(old.relative_to(root)),purpose='Unchanged B28+C5 gate for the cumulative Probe/Eliminate root-closure candidate against production.',
    hypothesis='General phase-entry restoration preserves all verdicts and avoids unacceptable cumulative regression.',
    build_identity='Independent immutable Release 5430/5432, identical hashes, 78/78 tests each; trace/conflict-stop/shadow/journal disabled.')
finish(c,m,'pin_sha256')
