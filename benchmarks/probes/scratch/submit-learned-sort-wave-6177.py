#!/usr/bin/env python3
"""Submit the next predeclared pair after expected outcomes, not proof checking."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260909-6176-learned-sort-rank4-runtime'
wave = int(sys.argv[1])
assert wave in (0, 1, 2)
p = c / f'wave{wave}-submission.json'
assert not p.exists(), 'already submitted; monitor original job'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['protected_campaign_hashes'].items():
    assert sha(root / rel) == digest, rel
parent = root / m['predecessor_rank4_record']['path']
assert sha(parent) == m['predecessor_rank4_record']['sha256']
old = json.loads(parent.read_text())
rows = {}
for folder in ('provisional', 'results'):
    for path in (c / folder).glob('*.json'):
        r = json.loads(path.read_text())
        assert classify(r, folder == 'results')['status'] in ('pending_validation', 'checked_UNSAT')
        for key in ('certificate_sha256', 'certificate_bytes', 'verdict', 'instance'):
            assert r[key] == old[key], (path, key)
        nt = lambda v: [s for s in v['solver_statistics'] if not s.startswith('c timing')]
        assert nt(r) == nt(old), path
        rows[r['task']] = path
assert set(rows) == set(range(2 * wave)), 'previous wave solver outcomes not complete'
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', f'--array={2*wave}-{2*wave+1}', str(c / 'array.sbatch')]
job = subprocess.run(command, capture_output=True, text=True, check=True).stdout.strip().split(';')[0]
assert job.isdigit()
p.write_text(json.dumps(dict(job_id=job, tasks=[2*wave, 2*wave+1], command=command,
    accounting_since=since, manifest_sha256=sha(c / 'manifest.json'),
    predecessor_solver_records={str(p.relative_to(root)): sha(p) for p in rows.values()}), indent=2) + '\n')
print(job, since)
