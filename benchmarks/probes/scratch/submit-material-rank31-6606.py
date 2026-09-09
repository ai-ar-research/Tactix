#!/usr/bin/env python3
"""Submit the owned candidate-only prefix after revalidating dependencies."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260909-6605-material-rank31-prefix'
target = c / 'launch.json'; assert not target.exists(), 'Already submitted'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items(): assert sha(root / rel) == digest, rel
for p in (root / 'benchmarks/runs/20260909-6594-material-ranks1-50/results').glob('*.json'):
    s = classify(json.loads(p.read_text()), True); assert s['checked'] and s['clean'], (p, s)
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
job = subprocess.check_output(command, text=True).strip().split(';')[0]; assert job.isdigit()
with target.open('x') as out:
    json.dump(dict(job_id=job, accounting_since=since, command=command,
        manifest_sha256=sha(c / 'manifest.json')), out, indent=2)
print(job, since)
