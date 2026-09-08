#!/usr/bin/env python3
"""Submit the prepared immutable two-arm diagnostic once."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5735-root-reuse-rank31-prefix'
p = c / 'submission.json'
assert not p.exists(), 'Already submitted; observe the recorded handle'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for name, digest in m['hashes'].items(): assert sha(c / name) == digest
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal(), job
p.write_text(json.dumps(dict(job=job, command=command, manifest_sha256=sha(c / 'manifest.json'),
    recorded_at=datetime.datetime.now(datetime.timezone.utc).isoformat()), indent=2) + '\n')
print('Submitted', job)
