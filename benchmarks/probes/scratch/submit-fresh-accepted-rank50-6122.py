#!/usr/bin/env python3
"""Submit the explicitly requested full fresh100-task paired reevaluation."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
target=c/'launch.json';assert not target.exists()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest,rel
for rel,digest in m['protected_campaign_hashes'].items():assert sha(root/rel)==digest,rel
command=['sbatch','--parsable',str(c/'array.sbatch')]
p=subprocess.run(command,capture_output=True,text=True)
assert p.returncode==0,(p.stdout,p.stderr)
job=p.stdout.strip().split(';')[0];assert job.isdigit()
target.write_text(json.dumps(dict(job_id=job,command=command,manifest_sha256=sha(c/'manifest.json'),
    continuation='Fresh accepted-production/current-Kissat pair onall50;separateimmutable100tasks,12CPU48000MiB1200solver16000checker5h. No other campaign orfixedreference changed;proof checking stays asynchronous.'),indent=2)+'\n')
print(job)
