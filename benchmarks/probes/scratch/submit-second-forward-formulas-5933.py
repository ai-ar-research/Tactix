#!/usr/bin/env python3
"""Submit only the two new immutable second-forward snapshot campaigns."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
for name in ('20260908-5931-second-forward-begin-formula','20260908-5932-second-forward-end-formula'):
    c = root / 'benchmarks/runs' / name
    target = c / 'launch.json'
    assert not target.exists()
    m = json.loads((c / 'manifest.json').read_text())
    for rel,digest in m['hashes'].items(): assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == digest
    command = ['sbatch','--parsable',str(c / 'array.sbatch')]
    r = subprocess.run(command,capture_output=True,text=True,check=True)
    job = r.stdout.strip().split(';')[0]
    assert job.isdigit(),r.stdout
    target.write_text(json.dumps(dict(job_id=job,command=command,
        manifest_sha256=hashlib.sha256((c / 'manifest.json').read_bytes()).hexdigest(),
        scope='Read-only formula observers on frozen5920/native pins;normal campaigns untouched.'),indent=2)+'\n')
    print(name,job,flush=True)
