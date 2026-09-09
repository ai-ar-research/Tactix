#!/usr/bin/env python3
"""Submit a prepared follow-up exactly once with protected campaign hashes."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
names = {'prefix':'20260909-6365-compact-stamp-rank32-prefix',
         'full':'20260909-6366-compact-stamp-ranks1-50'}
assert len(sys.argv) == 2 and sys.argv[1] in names
c = root/'benchmarks/runs'/names[sys.argv[1]]
assert not (c/'launch.json').exists()
m = json.loads((c/'manifest.json').read_text())
for rel, digest in m.get('pin_sha256', m.get('hashes', {})).items():
    assert sha(c/rel) == digest
gate = m['provisional_gate']
assert sha(root/gate['path']) == gate['sha256']
protected = json.loads((root/'benchmarks/runs/20260909-6332-collapse-three-wave-runtime/manifest.json').read_text())['protected_campaign_hashes']
for rel, digest in protected.items():
    assert sha(root/rel) == digest
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', str(c/'array.sbatch')]
job = subprocess.run(command, capture_output=True, text=True, check=True).stdout.strip().split(';')[0]
assert job.isdigit()
(c/'launch.json').write_text(json.dumps(dict(job_id=job, command=command,
    accounting_since=since, manifest_sha256=sha(c/'manifest.json'),
    protected_campaign_hashes=protected), indent=2)+'\n')
print(job, since)
