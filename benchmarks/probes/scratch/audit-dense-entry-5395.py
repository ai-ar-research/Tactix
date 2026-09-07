#!/usr/bin/env python3
"""Freeze a read-only scoped inventory; preserve every unique or active artifact."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
jobs = {5380: 1668924, 5383: 1669005, 5384: 1669085, 5385: 1669087,
        5386: 1669093, 5387: None, 5388: 1669094, 5389: 1669095,
        5390: 1669107, 5391: None, 5392: 1669173, 5393: 1669174,
        5394: 1669214, 5395: 1669252}

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()

campaigns = []
for number, job in jobs.items():
    c = next((root / 'benchmarks/runs').glob(f'20260907-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, expected in m.get('pin_sha256', m.get('hashes', {})).items():
        assert sha(c / rel) == expected, (c, rel)
    for rel, expected in m.get('evidence_sha256', {}).items():
        assert sha(c / rel) == expected, (c, rel)
    residue = [str(p.relative_to(root)) for p in c.rglob('*')
               if p.is_file() and p.suffix in ('.cnf', '.drat', '.lrat', '.grat', '.frat')]
    assert not residue, residue
    records = sorted((c / 'results').glob('*.json'))
    state = None
    if job:
        r = subprocess.run(['sacct', '-j', str(job), '-X',
                            '--format=JobID,State,ExitCode', '--parsable2'],
                           capture_output=True, text=True, check=True)
        state = r.stdout.splitlines()
    authorities = [p for p in (c / 'pins').glob('*') if p.is_file()]
    authorities += [p for folder in ('profiles', 'traces')
                    for p in (c / folder).glob('*') if p.is_file()]
    authorities += [p for p in (c / 'logs').glob('*.tsv') if p.is_file()]
    campaigns.append(dict(
        campaign=str(c.relative_to(root)), job=job, slurm_state=state,
        manifest_sha256=sha(c / 'manifest.json'),
        parent=m.get('parent_campaign', m.get('parent_source', m.get('predecessor'))),
        final_results_at_checkpoint=len(records),
        final_sha256={p.name: sha(p) for p in records},
        authority_sha256={str(p.relative_to(c)): dict(bytes=p.stat().st_size, sha256=sha(p))
                          for p in authorities},
        repository_residue=residue,
        retention='Preserve: active job or unique causal/checked evidence; referenced by dense-entry report, current PROGRESS and successor manifests. Parent pins/builds remain immutable.'))

result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              scope='5384–5395 plus preceding 5380/5383. No fuzz campaign artifacts inspected.',
              campaigns=campaigns, removed_paths=[], removed_bytes=0,
              cleanup='No safe deletion candidate established. Active external scratch remains job-owned; completed result cleanup is checked by retained sealers.')
p = root / 'benchmarks/reports/2026-09-07-dense-entry-artifact-index.json'
assert not p.exists(), 'Frozen inventory already exists'
p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: 14 retained campaigns, pin/evidence hashes verified, no repository certificate residue or manual deletion')
