#!/usr/bin/env python3
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
c = root/'benchmarks/runs/20260909-6787-rank38-watermark-profile'
target = c/'launch.json'
assert not target.exists()
m = json.loads((c/'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(c/rel) == digest, rel
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items():
        assert sha(root/rel) == digest, rel
for record in m['unprofiled_records'].values():
    assert sha(root/record['path']) == record['sha256']
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', str(c/'array.sbatch')]
job = subprocess.check_output(command, text=True).strip().split(';')[0]
assert job.isdigit()
with target.open('x') as stream:
    json.dump(dict(job_id=job, accounting_since=since, command=command,
        manifest_sha256=sha(c/'manifest.json')), stream, indent=2)
    stream.write('\n')
print(job, since)
