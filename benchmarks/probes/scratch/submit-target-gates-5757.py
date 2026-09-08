#!/usr/bin/env python3
"""Submit only prepared immutable target gates, never an existing submission."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
for name in ('20260908-5754-reuse-target-BC-gate','20260908-5755-reuse-target-rank31-replay','20260908-5756-reuse-target-rank33-replay'):
    c = root / 'benchmarks/runs' / name
    if (c / 'submission.json').exists():
        print('Already submitted',name)
        continue
    m = json.loads((c / 'manifest.json').read_text())
    for rel,digest in m.get('hashes',m.get('pin_sha256',{})).items():
        assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == digest,rel
    command = ['sbatch','--parsable',str((c / 'array.sbatch').relative_to(root))]
    job = subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip()
    assert job.isdecimal(),job
    (c / 'submission.json').write_text(json.dumps(dict(job=job,command=command,
        manifest_sha256=hashlib.sha256((c / 'manifest.json').read_bytes()).hexdigest()),indent=2)+'\n')
    print(name,job,flush=True)
