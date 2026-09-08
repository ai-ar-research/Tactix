#!/usr/bin/env python3
"""Submit the candidate-only full view after expected B28 solver outcomes."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root / 'benchmarks/runs/20260908-5758-reuse-target-ranks1-40'
if (c / 'submission.json').exists():
    print('Already submitted; no new job')
    raise SystemExit(0)
gate=root / 'benchmarks/runs/20260908-5754-reuse-target-BC-gate'
records={}
for folder in ('provisional','results'):
    for p in (gate / folder).glob('*.json'):
        r=json.loads(p.read_text())
        assert r['expected_match'] and r['solve']['exit_code'] in (10,20) and not r['solve']['timed_out']
        assert 'valid' not in r or r['valid']
        records[int(r['task'])]=dict(path=str(p.relative_to(root)),sha256=hashlib.sha256(p.read_bytes()).hexdigest())
if not set(range(56)).issubset(records):
    print('B28 solver outcomes pending; no submission')
    raise SystemExit(0)
m=json.loads((c / 'manifest.json').read_text())
assert set(m['arms']) == {'tactix'}
assert m['resource_contract']['array'] == '0-39'
for rel,digest in m['pin_sha256'].items():
    assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == digest
command=['sbatch','--parsable',str((c / 'array.sbatch').relative_to(root))]
job=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job,command=command,
    manifest_sha256=hashlib.sha256((c / 'manifest.json').read_bytes()).hexdigest(),
    regression_solver_authority=records,
    limitation='Investigation measurement while trace and certificate gates continue; no acceptance or isolated unit-cost claim.'),indent=2)+'\n')
print('Submitted40 candidate jobs,zero Kissat jobs:',job)
