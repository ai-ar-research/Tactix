#!/usr/bin/env python3
"""Submit only the three immutable ownership diagnostics."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6129-publication-memory'
target = c / 'launch.json'
assert not target.exists()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['protected_campaign_hashes'].items():
    assert sha(root / rel) == digest, rel
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
p = subprocess.run(command, capture_output=True, text=True, check=True)
job = p.stdout.strip().split(';')[0]
assert job.isdigit(), p.stdout
target.write_text(json.dumps(dict(job_id=job, command=command,
    manifest_sha256=sha(c / 'manifest.json'),
    continuation='ThreeTactix-only memory diagnostics,12CPU48000MiB1200solver16000checker5h;no othercampaign changed.'), indent=2) + '\n')
print(job)
