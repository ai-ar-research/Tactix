#!/usr/bin/env python3
"""Submit the isolated full50 transfer measurement once, protecting old jobs."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50'
assert not (c / 'launch.json').exists()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
for rel, digest in m['evidence_sha256'].items(): assert sha(root / rel) == digest, rel
protected = json.loads((root / 'benchmarks/runs/20260909-6332-collapse-three-wave-runtime/manifest.json').read_text())['protected_campaign_hashes']
for rel, digest in protected.items(): assert sha(root / rel) == digest, rel
since = dt.datetime.now().strftime('%Y-%m-%dT%H:%M:%S')
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
job = subprocess.run(command, capture_output=True, text=True, check=True).stdout.strip().split(';')[0]
assert job.isdigit()
with (c / 'launch.json').open('x') as out:
    json.dump(dict(job_id=job, command=command, accounting_since=since,
        manifest_sha256=sha(c / 'manifest.json'), protected_campaign_hashes=protected,
        deadline_authority='User explicitly resumed after the old Sep10 06:00 cutoff; no new cutoff received.'), out, indent=2)
    out.write('\n')
print(job, since)
