#!/usr/bin/env python3
"""Pin current causal and regression gates and predeclare three Rank33 timing pairs."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260908-5626-congruence-index-validated'
a = json.loads((authority / 'manifest.json').read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for name,digest in a['binaries'].items(): assert sha(root/name) == digest
for name,digest in a['patches'].items(): assert sha(authority/name) == digest
builds = {arm:root/('build/pin-congruence-index-repair-5623-'+arm) for arm in ('release','bc','trace')}
def initialize(old,name,diagnostic=False):
    m=json.loads((old/'manifest.json').read_text())
    for rel,digest in m.get('hashes',m.get('pin_sha256',{})).items(): assert sha(old/rel)==digest
    c=runs/name
    c.mkdir(exist_ok=False)
    for folder in (('pins','logs','results','traces') if diagnostic else ('pins','logs','results','provisional','claims')):
        (c/folder).mkdir()
    for p in (old/'pins').iterdir():
        if p.is_file():
            shutil.copy2(p,c/'pins'/p.name)
            (c/'pins'/p.name).chmod(p.stat().st_mode|0o200)
    file='run.py' if diagnostic else 'matrix.tsv'
    (c/file).write_text((old/file).read_text().replace('tactix-5611-','tactix-5627-'))
    (c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
    m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        predecessor=str(old.relative_to(root)),candidate_authority=str(authority.relative_to(root)),
        test_consideration=a['test_consideration'])
    m.pop('slurm_job_id',None)
    return c,m
def finish(c,m,diagnostic=False,submit=False):
    names=[*(p for p in (c/'pins').iterdir() if p.is_file()),c/'array.sbatch',c/('run.py' if diagnostic else 'matrix.tsv')]
    m['hashes' if diagnostic else 'pin_sha256']={str(p.relative_to(c)):sha(p) for p in names}
    (c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
    if submit:
        command=['sbatch','--parsable',str((c/'array.sbatch').relative_to(root))]
        job=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip()
        assert job.isdecimal()
        (c/'submission.json').write_text(json.dumps(dict(job=job,command=command,manifest_sha256=sha(c/'manifest.json')),indent=2)+'\n')
        print('Submitted',c.name,job)
    else: print('Prepared',c.name)
old=runs/'20260908-5611-current-congruence-output'
c,m=initialize(old,'20260908-5627-congruence-index-rank33-prefix',True)
shutil.copy2(builds['trace']/'tactix',c/'pins/candidate')
m.update(parent_campaign=str(old.relative_to(root)),parent_summary_sha256=sha(old/'clause-localization.json'),
    candidate_source=a['source'],source_patch_sha256=sha(authority/'cumulative.patch'),
    hypothesis='Applying root-satisfied index cleanup should close the five retained Congruence9457 ternaries on current Rank33. Validate actual whole100K decisions and formula outputs; do not assume later182093 split closes.',
    validation='Two clean100K stops and complete hashes. Native whole5611 traces/proof replay; candidate may change. Full candidate/native decision comparison and exact framed formula/binary outputs. No acceptance timing or complete verdict.')
finish(c,m,True,True)
old=runs/'20260908-5598-factor-support-BC-gate'
c,m=initialize(old,'20260908-5628-congruence-index-BC-gate')
for tier,build in [('B',builds['release']),('C',builds['bc'])]:
    arm='candidate_'+tier
    shutil.copy2(build/'tactix',c/'pins'/arm)
    m['arms'][arm].update(sha256=sha(c/'pins'/arm),source_build=str(build.relative_to(root)),source_patch_sha256=sha(authority/'cumulative.patch'))
shutil.copy2(authority/'cumulative.patch',c/'pins/candidate.patch')
m.update(purpose='Unchanged B28+C5 cumulative regression gate for general Congruence index cleanup.',
    hypothesis='Retire only visited root-satisfied records with valid deletions and preserve all expected verdicts/cumulative gate behavior.',
    build_identity='Independent repaired Release/BC88/88 each,matching binary SHA;original fixture failure explained and fixed,solver unchanged by review.',
    status='UNFUZZED isolated candidate;pending checks permit diagnosis,not promotion.')
finish(c,m,False,True)
old=runs/'20260908-5599-factor-support-rank32-runtime'
c,m=initialize(old,'20260908-5629-congruence-index-rank33-runtime')
for arm,build,auth in [('candidate',builds['release'],authority),
                       ('previous',root/'build/pin-5592-factor-root-support-release',runs/'20260908-5592-factor-root-support')]:
    shutil.copy2(build/'tactix',c/'pins'/arm)
    shutil.copy2(auth/'cumulative.patch',c/'pins'/(arm+'.patch'))
    m['arms'][arm].update(sha256=sha(c/'pins'/arm),source_build=str(build.relative_to(root)),source_patch_sha256=sha(auth/'cumulative.patch'))
full=runs/'20260908-5604-factor-support-ranks1-40'
selected=json.loads((full/'next-losses.json').read_text())
assert selected['losses'][0]['rank']==33
with (full/'matrix.tsv').open(newline='') as f: target=next(row for row in csv.DictReader(f,delimiter='\t') if row['rank']=='33')
with (old/'matrix.tsv').open(newline='') as f:
    reader=csv.DictReader(f,delimiter='\t'); fields=reader.fieldnames; rows=list(reader)
for row in rows:
    for key in ('rank','instance','expected','tier'):row[key]=target[key]
with (c/'matrix.tsv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fields,delimiter='\t',lineterminator='\n');writer.writeheader();writer.writerows(rows)
fm=json.loads((full/'manifest.json').read_text())
m.update(input_sha256={target['instance']:fm['input_sha256'][target['instance']]},
    parent_campaign=str(full.relative_to(root)),selection_authority=dict(path=str((full/'next-losses.json').relative_to(root)),sha256=sha(full/'next-losses.json')),
    hypothesis='Measure net Rank33 runtime effect of visited satisfied-clause index cleanup against actual5592 on the largest full-window loss. No tuned schedule or budget.',
    decision='After all B28 expected solver records,submit predeclared waves0,1,2 after predecessor solves without waiting for checkers. Require within-arm complete non-timing statistics/proof identity,all ratios and spreads<=2%.Cross-arm work may change;full Rank1-40 remains required.',
    candidate_validation=a['test_consideration'],status='UNFUZZED isolated candidate; no promotion.')
finish(c,m)
