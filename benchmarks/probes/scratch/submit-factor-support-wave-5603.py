#!/usr/bin/env python3
"""Submit each predeclared timing wave after solver completion, not proof checking."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5599-factor-support-rank32-runtime'
gate = root / 'benchmarks/runs/20260908-5598-factor-support-BC-gate'
wave = int(sys.argv[1])
assert wave in (0, 1, 2)
p = c / f'wave{wave}-submission.json'
if p.exists():
    print('Already submitted:', p.read_text()); raise SystemExit(0)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def expected(r):
    assert r['expected_match'] and r['solve']['exit_code'] in (10, 20) and not r['solve']['timed_out']
    if 'valid' in r:
        assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
        check = r['model_check'] if r['solve']['exit_code'] == 10 else r['proof_check']
        assert check['exit_code'] == 0 and not check['timed_out']
def records(campaign):
    rows, paths = {}, {}
    for folder in ('provisional', 'results'):
        for f in (campaign / folder).glob('*.json'):
            r = json.loads(f.read_text()); expected(r)
            rows[r['task']], paths[r['task']] = r, f
    return rows, paths
m = json.loads((c / 'manifest.json').read_text())
for name, digest in m['pin_sha256'].items(): assert sha(c / name) == digest
bound = {}
if wave == 0:
    rows, paths = records(gate)
    if not set(range(56)).issubset(rows):
        print('B28 solver snapshots pending:', len(set(range(56)) & rows.keys()), 'of56')
        raise SystemExit(0)
    bound = {str(paths[i].relative_to(root)):sha(paths[i]) for i in range(56)}
else:
    assert (c / f'wave{wave-1}-submission.json').exists()
    rows, paths = records(c)
    needed = range(2 * wave)
    if not set(needed).issubset(rows):
        print('Preceding solver snapshots pending'); raise SystemExit(0)
    stats = lambda r:[line for line in r['solver_statistics'] if not line.startswith('c timing')]
    for i in needed:
        r = rows[i]
        assert r['verdict'] == 'UNSAT' and r['solve']['exit_code'] == 20
        assert r['arm'] == ('candidate' if i % 2 == 0 else 'previous')
        assert stats(r) == stats(rows[i % 2])
        assert r['certificate_sha256'] == rows[i % 2]['certificate_sha256']
        bound[str(paths[i].relative_to(root))] = sha(paths[i])
tasks = [2 * wave, 2 * wave + 1]
command = ['sbatch', '--parsable', f'--array={tasks[0]}-{tasks[1]}', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal(), job
p.write_text(json.dumps(dict(job=job, tasks=tasks, command=command,
    recorded_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    manifest_sha256=sha(c / 'manifest.json'), predecessor_solver_records=bound,
    validation='Expected solver outcomes permit continuation; proof/model checks remain asynchronous and must pass before promotion.'), indent=2) + '\n')
print('Submitted', job, tasks)
