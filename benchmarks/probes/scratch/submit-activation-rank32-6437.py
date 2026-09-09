#!/usr/bin/env python3
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6436-activation-rank32-prefix'; assert not (c/'launch.json').exists()
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
for key in ('candidate_authority','cached_native','provisional_gate','preserved_closure'):
    b=m[key]; assert sha(root/b['path'])==b['sha256']
protected=json.loads((root/'benchmarks/runs/20260909-6332-collapse-three-wave-runtime/manifest.json').read_text())['protected_campaign_hashes']
for rel,digest in protected.items(): assert sha(root/rel)==digest
since=dt.datetime.now().replace(microsecond=0).isoformat()
command=['sbatch','--parsable',str(c/'array.sbatch')]
job=subprocess.run(command,text=True,capture_output=True,check=True).stdout.strip().split(';')[0]; assert job.isdigit()
(c/'launch.json').write_text(json.dumps(dict(job_id=job,command=command,accounting_since=since,
    manifest_sha256=sha(c/'manifest.json'),protected_campaign_hashes=protected),indent=2)+'\n')
print(job,since)
