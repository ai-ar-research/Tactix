#!/usr/bin/env python3
"""Submit separately pinned cumulative gates; preserve any successful handle."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
for name in ('20260908-6114-substitute-publication-BC-gate','20260908-6115-substitute-publication-ranks1-50'):
    c=root/'benchmarks/runs'/name
    target=c/'launch.json'
    if target.exists():
        print('Already submitted',name,json.loads(target.read_text())['job_id']);continue
    m=json.loads((c/'manifest.json').read_text())
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest,rel
    command=['sbatch','--parsable',str(c/'array.sbatch')]
    p=subprocess.run(command,capture_output=True,text=True)
    assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
    job=p.stdout.strip().split(';')[0];assert job.isdigit(),p.stdout
    target.write_text(json.dumps(dict(job_id=job,command=command,manifest_sha256=sha(c/'manifest.json'),
        continuation='Independent12CPU48000MiB1200solver16000checker5h full-width gate. Continue while proofscheck. No prior source,pin,runner or fixed timing changed.'),indent=2)+'\n')
    print(name,job,flush=True)
