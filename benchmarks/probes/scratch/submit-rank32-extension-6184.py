#!/usr/bin/env python3
"""Submit the bounded current/native Rank32 extension without touching timing pins."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6183-rank32-current-prefix'
assert not (c / 'launch.json').exists()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['earlier_closure'].items():
    assert sha(root / rel) == digest, rel
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
job = subprocess.run(command, capture_output=True, text=True, check=True).stdout.strip().split(';')[0]
assert job.isdigit()
(c / 'launch.json').write_text(json.dumps(dict(job_id=job, command=command,
    accounting_since=since, manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print(job, since)
