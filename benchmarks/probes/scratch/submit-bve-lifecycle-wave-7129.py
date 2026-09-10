#!/usr/bin/env python3
"""Submit one predeclared matched profile pair; never wait for proof checks."""
import datetime
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
source = Path(__file__).with_name('validate-bve-lifecycle-profiles-7130.py')
spec = importlib.util.spec_from_file_location('profile_gate', source)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
assert len(sys.argv) == 2 and sys.argv[1] in ('0', '1', '2')
wave = int(sys.argv[1]); c = h.c
target = c / f'wave{wave}-submission.json'
assert not target.exists(), 'already submitted; monitor existing job'
out = h.read_profiles()
assert set(out['rows']) == set(range(2 * wave)), 'preceding profile solver pair incomplete'
since = datetime.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', f'--array={2*wave}-{2*wave+1}', str(c / 'array.sbatch')]
job = subprocess.check_output(command, text=True).strip().split(';')[0]
assert job.isdigit()
with target.open('x') as stream:
    json.dump(dict(job_id=job, accounting_since=since, command=command,
        tasks=list(range(2 * wave, 2 * wave + 2)), manifest_sha256=h.sha(c / 'manifest.json'),
        preceding_hashes=out['hashes'], submit_helper_sha256=h.sha(Path(__file__))), stream, indent=2)
print(job, since)
