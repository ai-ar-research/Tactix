#!/usr/bin/env python3
"""One explicit successor diagnostic submission."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5883-rank32-choice-frontier'
assert not (c / 'launch.json').exists()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == digest
cmd = ['sbatch', '--parsable', str(c / 'array.sbatch')]
r = subprocess.run(cmd, text=True, capture_output=True, check=True)
job = r.stdout.strip().split(';')[0]
assert job.isdigit()
(c / 'launch.json').write_text(json.dumps(dict(job_id=job, command=cmd,
    manifest_sha256=hashlib.sha256((c / 'manifest.json').read_bytes()).hexdigest()), indent=2) + '\n')
print(job)
