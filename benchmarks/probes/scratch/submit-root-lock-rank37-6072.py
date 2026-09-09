#!/usr/bin/env python3
"""Submit only the newly pinned current Rank37 diagnostic."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6070-root-lock-rank37-prefix'
target = c / 'launch.json'
assert not target.exists()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == digest, rel
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
p = subprocess.run(command, capture_output=True, text=True, check=True)
job = p.stdout.strip().split(';')[0]
assert job.isdigit(), p.stdout
target.write_text(json.dumps(dict(job_id=job, command=command,
    manifest_sha256=hashlib.sha256((c / 'manifest.json').read_bytes()).hexdigest(),
    continuation='Current6049 diagnostic only;full6061 checks and cpu-q campaign untouched. Cached native reused.'), indent=2) + '\n')
print(job)
