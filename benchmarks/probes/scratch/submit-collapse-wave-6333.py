#!/usr/bin/env python3
"""Require exact per-arm work from preceding waves; pending checks do not block."""
import csv
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260909-6332-collapse-three-wave-runtime'
wave=int(sys.argv[1]);assert wave in (0,1,2)
p=c/f'wave{wave}-submission.json';assert not p.exists(),'Already submitted; monitor existing job'
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest
for rel,digest in m['protected_campaign_hashes'].items():assert sha(root/rel)==digest
for binding in m['authorities'].values():assert sha(root/binding['path'])==binding['sha256']
with (c/'matrix.tsv').open() as inp:matrix=list(csv.DictReader(inp,delimiter='\t'))
rows={}
for folder in ('provisional','results'):
    for q in (c/folder).glob('*.json'):
        r=json.loads(q.read_text());assert classify(r,folder=='results')['status'] in ('pending_validation','checked_UNSAT'),q
        row=matrix[r['task']]
        for key in ('arm','instance','expected'):assert r[key]==row[key],(q,key)
        assert r['rank']==int(row['rank'])
        binding=m['work_oracles'][r['arm']][str(r['rank'])]
        assert sha(root/binding['path'])==binding['sha256']
        old=json.loads((root/binding['path']).read_text())
        for key in ('certificate_sha256','certificate_bytes','verdict','instance'):assert r[key]==old[key],(q,key)
        nt=lambda v:[s for s in v['solver_statistics'] if not s.startswith('c timing')]
        assert nt(r)==nt(old),q
        rows[r['task']]=q
assert set(rows)==set(range(6*wave)),'Previous wave expected outcomes not complete'
since=dt.datetime.now().replace(microsecond=0).isoformat()
command=['sbatch','--parsable',f'--array={6*wave}-{6*wave+5}',str(c/'array.sbatch')]
job=subprocess.run(command,capture_output=True,text=True,check=True).stdout.strip().split(';')[0];assert job.isdigit()
p.write_text(json.dumps(dict(job_id=job,tasks=list(range(6*wave,6*wave+6)),command=command,accounting_since=since,
    manifest_sha256=sha(c/'manifest.json'),preceding_records={str(q.relative_to(root)):sha(q) for q in rows.values()}),indent=2)+'\n')
print(job,since)
