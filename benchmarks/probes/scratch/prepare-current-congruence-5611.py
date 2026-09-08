#!/usr/bin/env python3
"""Rebind the earlier Rank33 clause witness to the actual current candidate."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5582-rank33-congruence-output'
c = root / 'benchmarks/runs/20260908-5611-current-congruence-output'
a = root / 'benchmarks/runs/20260908-5592-factor-root-support'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
m = json.loads((old / 'manifest.json').read_text())
auth = json.loads((a / 'manifest.json').read_text())
pin = root / 'build/pin-5593-factor-root-support-trace/tactix'
assert sha(pin) == auth['binaries'][str(pin.relative_to(root))]
for name, digest in m['hashes'].items(): assert sha(old / name) == digest
assert (old / 'clause-localization.json').is_file()
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
shutil.copy2(pin, c / 'pins/candidate')
shutil.copy2(old / 'pins/kissat', c / 'pins/kissat')
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5582-', 'tactix-5611-'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)),
    parent_summary_sha256=sha(old / 'clause-localization.json'),
    candidate_authority=str(a.relative_to(root)), candidate_source=auth['source'],
    source_patch_sha256=sha(a / 'cumulative.patch'),
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    hypothesis='Determine whether the five Congruence9457 retained ternaries remain on current5592 before investigating their lifecycle. Previous5522 witness alone is insufficient after a trajectory-changing Factor repair.',
    validation='Two clean100K stops; full current decision streams and prefix consistency. Native whole traces and partial proof must replay5582. Candidate may change; record full per-artifact identity to5582 without assuming it. Reuse framed clause/binary parsers.',
    test_consideration='Configuration-only binding of tested immutable observers. Existing19 Factor parser fixtures and8 Congruence selector fixtures plus pin/trace/proof/full-stream assertions cover this change; no solver unit-test change.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job, command=command,
    manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print('Submitted current-source Congruence witness:', job)
