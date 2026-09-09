#!/usr/bin/env python3
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
for name in ('20260909-6236-substitute-retry-BC-gate','20260909-6237-substitute-retry-rank32-prefix'):
    c=root/'benchmarks/runs'/name
    assert not (c/'launch.json').exists()
    m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',m.get('hashes')).items(): assert sha(c/rel)==digest
    for rel,digest in m.get('protected_campaign_hashes',{}).items(): assert sha(root/rel)==digest
    since=dt.datetime.now().replace(microsecond=0).isoformat()
    command=['sbatch','--parsable',str(c/'array.sbatch')]
    job=subprocess.check_output(command,text=True).strip().split(';')[0]; assert job.isdigit()
    (c/'launch.json').write_text(json.dumps(dict(job_id=job,command=command,accounting_since=since,manifest_sha256=sha(c/'manifest.json')),indent=2)+'\n')
    print(name,job,since,flush=True)
