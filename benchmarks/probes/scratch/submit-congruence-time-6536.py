#!/usr/bin/env python3
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6535-rank32-congruence-time'
assert not (c / 'launch.json').exists()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
for binding in m['authorities'].values(): assert sha(root / binding['path']) == binding['sha256']
for rel, digest in m['protected_campaign_hashes'].items(): assert sha(root / rel) == digest, rel
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
job = subprocess.check_output(command, text=True).strip().split(';')[0]
assert job.isdigit()
with (c / 'launch.json').open('x') as out:
    json.dump(dict(job_id=job, command=command, accounting_since=since,
                   manifest_sha256=sha(c / 'manifest.json')), out, indent=2)
    out.write('\n')
print(job, since)
