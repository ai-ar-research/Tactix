#!/usr/bin/env python3
"""Submit each prepared immutable prefix exactly once, independently."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for number, rank in ((6139, 21), (6140, 4)):
    c = root / f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'
    target = c / 'launch.json'
    assert not target.exists(), target
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items():
        assert sha(c / rel) == digest, rel
    command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
    p = subprocess.run(command, capture_output=True, text=True, check=True)
    job = p.stdout.strip().split(';')[0]
    assert job.isdigit(), p.stdout
    target.write_text(json.dumps(dict(job_id=job, command=command,
        manifest_sha256=sha(c / 'manifest.json'),
        continuation='Shortpriorityprefix;allpriorjobs,pins andfixedreferenceunchanged.'), indent=2) + '\n')
    print(number, job, flush=True)
