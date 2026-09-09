#!/usr/bin/env python3
"""Submit only new6305 full50/prefix campaigns, never touch inherited arrays."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
protected=json.loads((root/'benchmarks/runs/20260909-6176-learned-sort-rank4-runtime/manifest.json').read_text())['protected_campaign_hashes']
for rel,digest in protected.items():assert sha(root/rel)==digest,rel
campaigns=[root/'benchmarks/runs'/name for name in ('20260909-6320-substitute-collapse-ranks1-50','20260909-6321-collapse-rank32-prefix')]
for c in campaigns:
    assert not (c/'launch.json').exists()
    m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',m.get('hashes',{})).items():assert sha(c/rel)==digest,rel
    for key in ('candidate_authority','fixed_reference','cached_native','provisional_gate'):
        if key in m:assert sha(root/m[key]['path'])==m[key]['sha256'],key
for c in campaigns:
    since=dt.datetime.now().replace(microsecond=0).isoformat()
    command=['sbatch','--parsable',str(c/'array.sbatch')]
    job=subprocess.run(command,capture_output=True,text=True,check=True).stdout.strip().split(';')[0]
    assert job.isdigit()
    (c/'launch.json').write_text(json.dumps(dict(job_id=job,command=command,accounting_since=since,
        manifest_sha256=sha(c/'manifest.json'),protected_campaign_hashes=protected),indent=2)+'\n')
    print(c.name,job,since,flush=True)
