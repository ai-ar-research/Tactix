#!/usr/bin/env python3
"""Submit only explicitly prepared new campaigns and capture immutable launches."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
names = {
    'gate': ['20260908-5855-forward-caller-BC-gate',
             '20260908-5856-forward-caller-rank31-prefix',
             '20260908-5857-forward-caller-rank32-prefix'],
    'full': ['20260908-5858-forward-caller-ranks1-40']}
for name in names[sys.argv[1]]:
    c = root / 'benchmarks/runs' / name
    target = c / 'launch.json'
    assert not target.exists()
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m.get('pin_sha256', m.get('hashes', {})).items():
        assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == digest
    command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
    run = subprocess.run(command, capture_output=True, text=True, check=True)
    job = run.stdout.strip().split(';')[0]
    assert job.isdigit(), run.stdout
    target.write_text(json.dumps(dict(job_id=job, command=command,
        manifest_sha256=hashlib.sha256((c / 'manifest.json').read_bytes()).hexdigest(),
        continuation='Independent5849 successor;existing campaigns unchanged. Pending expected verdicts permit development,not promotion.'), indent=2) + '\n')
    print(name, job, flush=True)
