#!/usr/bin/env python3
"""Submit the independent two-arm stopped diagnostic exactly once."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7011-rank35-current-native-prefix'
target = c / 'launch.json'
assert not target.exists(), 'already submitted'
m = json.loads((c / 'manifest.json').read_text())
assert m['session_authority'].startswith('Explicit user continue after Sep10 06:00EDT')
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['dependencies'].items():
    assert sha(root / rel) == digest, rel
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
since = dt.datetime.now().replace(microsecond=0).isoformat()
job = subprocess.check_output(command, text=True).strip().split(';')[0]
assert job.isdigit()
with target.open('x') as stream:
    json.dump(dict(job_id=job, accounting_since=since, command=command,
                   manifest_sha256=sha(c / 'manifest.json'),
                   submit_helper_sha256=sha(Path(__file__))), stream, indent=2)
    stream.write('\n')
print(job, since)
