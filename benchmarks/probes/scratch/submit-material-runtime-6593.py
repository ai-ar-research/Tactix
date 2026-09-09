#!/usr/bin/env python3
"""One independent paired wave; never restart existing jobs or await proofs."""
import csv, datetime, json, subprocess, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260909-6592-rank50-material-three-wave-runtime'
wave=int(sys.argv[1]); assert wave in (0,1,2)
target=c/f'wave{wave}-submission.json'; assert not target.exists()
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items(): assert sha(c/rel)==digest,rel
for rel,digest in m['dependencies'].items(): assert sha(root/rel)==digest,rel
for binding in m['authorities'].values(): assert sha(root/binding['path'])==binding['sha256']
prior=json.loads((root/'benchmarks/reports/2026-09-09-material-state-audit-6577.json').read_text())
for rel,digest in prior['protected_campaign_hashes'].items(): assert sha(root/rel)==digest,rel
matrix=list(csv.DictReader((c/'matrix.tsv').open(),delimiter='\t'))
records={}; bindings={}
for folder in ('provisional','results'):
    for path in sorted((c/folder).glob('*.json')):
        r=json.loads(path.read_text()); status=classify(r,folder=='results')
        assert status['status'] in ('pending_validation','checked_SAT'),(path,status)
        task=r['task']; row=matrix[task]
        for field in ('arm','instance','expected'): assert r[field]==row[field]
        assert r['rank']==int(row['rank'])==50
        if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
        records[task]=r; bindings[str(path.relative_to(root))]=sha(path)
assert set(records)==set(range(2*wave)), 'previous expected solver wave incomplete'
if wave:
    binding=m['previous_oracle']; assert sha(root/binding['path'])==binding['sha256']
    old=json.loads((root/binding['path']).read_text())
    for r in records.values():
        oracle=old if r['arm']=='previous' else records[0]
        for key in ('certificate_sha256','certificate_bytes','verdict','instance'): assert r[key]==oracle[key],key
        work=lambda x:[s for s in x['solver_statistics'] if not s.startswith('c timing ')]
        assert work(r)==work(oracle),r['task']
since=datetime.datetime.now().replace(microsecond=0).isoformat()
command=['sbatch','--parsable',f'--array={2*wave}-{2*wave+1}',str(c/'array.sbatch')]
job=subprocess.run(command,capture_output=True,text=True,check=True).stdout.strip().split(';')[0]
assert job.isdigit()
with target.open('x') as out:
    json.dump(dict(job_id=job,tasks=list(range(2*wave,2*wave+2)),command=command,accounting_since=since,
        manifest_sha256=sha(c/'manifest.json'),preceding_records=bindings),out,indent=2)
print(job,since)
