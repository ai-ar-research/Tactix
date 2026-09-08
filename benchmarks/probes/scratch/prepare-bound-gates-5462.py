#!/usr/bin/env python3
"""Pin three short B witnesses and unchanged prefix/full-view/B28+C5 gates."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3];runs=root/'benchmarks/runs'
authority=runs/'20260907-5459-bound-advance'
assert (authority/'manifest.json').exists()
release=root/'build/pin-5459-bound-advance-release/tactix'
bc=root/'build/pin-5461-bound-advance-bc/tactix'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(release)==sha(bc)
def init(c,dirs):
    c.mkdir(exist_ok=False)
    for d in dirs:(c/d).mkdir()
def finish(c,m,key):
    m.pop('slurm_job_id',None)
    m['created_at']=datetime.datetime.now(datetime.timezone.utc).isoformat()
    m['candidate_authority']=str(authority.relative_to(root))
    entries=[*(c/'pins').iterdir(),c/'array.sbatch']
    for n in ('run.py','matrix.tsv'):
        if (c/n).exists():entries.append(c/n)
    m[key]={str(p.relative_to(c)):sha(p) for p in entries}
    (c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
    print(c,flush=True)

old=runs/'20260907-5455-eliminate-limit-BC-gate'
c=runs/'20260907-5462-short-bound-witnesses'
init(c,('pins','logs','results','provisional','claims'))
m=json.loads((old/'manifest.json').read_text())
selected={'pyhala-braun-sat-35-4-04.shuffled.cnf.xz',
          'square.2.0.i.smt2-cvc4.cnf.xz','rbsat-v760c43649gyes10.cnf.xz'}
rows=[]
with (old/'matrix.tsv').open() as f:
    for row in csv.DictReader(f,delimiter='\t'):
        if row['arm']=='candidate_B' and Path(row['instance']).name in selected:
            for arm in ('predecessor','candidate','kissat'):
                rows.append(dict(row,task=str(len(rows)),arm=arm))
assert len(rows)==9
with (c/'matrix.tsv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]),delimiter='\t');writer.writeheader();writer.writerows(rows)
for name in ('drat-trim','checkmodel','modelcheck.py'):shutil.copy2(old/'pins'/name,c/'pins'/name)
m['arms']={}
for arm,binary in [('predecessor',old/'pins/candidate_B'),('candidate',release),
                  ('kissat',runs/'20260907-5454-eliminate-limit-ranks1-40/pins/kissat')]:
    shutil.copy2(binary,c/'pins'/arm)
    m['arms'][arm]=dict(path='pins/'+arm,sha256=sha(binary),
        arguments=['--verbose=3'] if arm=='kissat' else [],source_binary=str(binary.relative_to(root)))
shutil.copy2(authority/'cumulative.patch',c/'pins/candidate.patch')
runner=(old/'pins/runtime_task.py').read_text()
needle='        affinity = sorted(os.sched_getaffinity(0))'
assert runner.count(needle)==1
runner=runner.replace(needle,'        os.environ["TACTIX_SLOT_TRACE"] = "1"\n'+needle)
needle='        if context is not None:\n'
assert runner.count(needle)==1
runner=runner.replace(needle,'        shutil.copy2(stderr, CAMPAIGN / "logs" / f"{task:03d}.solver.stderr")\n'
    '        shutil.copy2(stdout, CAMPAIGN / "logs" / f"{task:03d}.solver.stdout")\n'+needle)
(c/'pins/runtime_task.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c))
    .replace('--array=0-65','--array=0-8').replace('5455-eliminate-limit-BC','5462-short-bound'))
m['input_sha256']={r['instance']:m['input_sha256'][r['instance']] for r in rows}
m.update(purpose='Short B mechanism witnesses: old/new bound lifecycle and native reference; full models/proofs checked, diagnostic not timing acceptance.',
    hypothesis='These fast but productive B instances exercise bound cuts and Factor bound rejection, permitting short causal replays.',
    selection='5435 candidate scout: pyhala 4.574s/84 Factor, square 5.275s/2455 Factor, rbsat 6.326s/2550 Factor. Selection is mechanism reach, not a new tier or tuned policy.',
    validation='Nine expected, independently checked and clean results; inspect slot and native verbose bound transitions. No competitiveness claim from this subset.',
    build_identity='Tested 5459 versus immutable 5450 and release Kissat; diagnostic logging enabled.',
    collect_runtime_context=False,predecessor=str(old.relative_to(root)))
m['resource_contract']['array']='0-8'
finish(c,m,'pin_sha256')

old=runs/'20260907-5453-rank37-eliminate-limit-prefix'
c=runs/'20260907-5463-rank37-bound-prefix'
init(c,('pins','logs','traces','results'))
shutil.copy2(root/'build/pin-5460-bound-advance-trace/tactix',c/'pins/candidate')
shutil.copy2(old/'pins/kissat',c/'pins/kissat')
(c/'run.py').write_text((old/'run.py').read_text().replace("'tactix-5453-'","'tactix-5463-'"))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
m=json.loads((old/'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),candidate_source='.claude/worktrees/runtime-bound-advance-5459',
    hypothesis='Restored bound advance should close the Factor gain-2 acceptance difference at conflict 32073. Compare full 100000-conflict prefix, not only counts.',
    validation='Two clean controlled stops, complete trace hashes and first decision divergence; no complete proof or timing claim.')
finish(c,m,'hashes')

for kind,oldname,newname in [
    ('bc','20260907-5455-eliminate-limit-BC-gate','20260907-5464-bound-advance-BC-gate'),
    ('full','20260907-5454-eliminate-limit-ranks1-40','20260907-5465-bound-advance-ranks1-40')]:
    old=runs/oldname;c=runs/newname
    init(c,('pins','logs','results','provisional','claims'))
    m=json.loads((old/'manifest.json').read_text())
    for p in (old/'pins').iterdir():
        if p.is_file():shutil.copy2(p,c/'pins'/p.name)
    for arm,info in m['arms'].items():
        if arm=='tactix' or arm.startswith('candidate_'):
            binary=bc if arm.endswith('_C') else release
            dest=c/info['path'];dest.chmod(0o755);shutil.copy2(binary,dest)
            info.update(sha256=sha(binary),source_build=str(binary.parent.relative_to(root)),
                        source_patch_sha256=sha(authority/'cumulative.patch'))
    patch=c/'pins'/('candidate.patch' if kind=='bc' else 'candidate-source.patch')
    patch.chmod(0o644);shutil.copy2(authority/'cumulative.patch',patch)
    shutil.copy2(old/'matrix.tsv',c/'matrix.tsv')
    (c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c))
        .replace('5455-eliminate-limit-BC','5464-bound-advance-BC'))
    m.update(parent_campaign=str(old.relative_to(root)),predecessor=str(old.relative_to(root)),
        purpose='Unchanged '+('B28+C5 cumulative production regression gate' if kind=='bc' else 'full Rank 1-40 paired Kissat comparison')+' for bound advancement repair.',
        hypothesis='Restore the general native bound/retry lifecycle after quota cuts; assess all runtime, work, memory and verdicts without selecting trajectories.',
        candidate_validation='Release 82/82 twice, trace 81/81, two actual old/new cut-bound fixtures and updated 12 phase cases; UNFUZZED.',
        build_identity='Independent tested Release 5459/5461 have matching hashes; diagnostic observers disabled.')
    finish(c,m,'pin_sha256')
