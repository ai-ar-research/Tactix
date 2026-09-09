#!/usr/bin/env python3
"""Submit the current Release pair with independent solver/checker budgets."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260909-6166-rank4-release-profile'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert not (c / 'launch.json').exists()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
for rel, digest in m['protected_campaign_hashes'].items(): assert sha(root / rel) == digest, rel
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
p = subprocess.run(command, capture_output=True, text=True, check=True)
job = p.stdout.strip().split(';')[0]
assert job.isdigit(), p.stdout
(c / 'launch.json').write_text(json.dumps(dict(job_id=job, command=command,
    manifest_sha256=sha(c / 'manifest.json'), accounting_since=since), indent=2) + '\n')
print(job, since)
