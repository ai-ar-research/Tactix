#!/usr/bin/env python3
"""Record an already-submitted wave, binding its predecessor solver snapshots."""
import datetime
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5551-vivify-census-rank39-runtime'
wave, job = int(sys.argv[1]), sys.argv[2]
assert wave in (1, 2) and job.isdecimal()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for name, digest in m['pin_sha256'].items(): assert sha(c / name) == digest
records = []
evidence = {}
for task in m['wave_tasks'][str(wave - 1)]:
    p = c / 'provisional' / f'{task:03d}.json'
    r = json.loads(p.read_text())
    assert r['task'] == task and r['expected_match']
    assert r['verdict'] == 'UNSAT' and r['solve']['exit_code'] == 20
    assert not r['solve']['timed_out']
    records.append(r)
    evidence[str(p.relative_to(c))] = sha(p)
assert [r['arm'] for r in records] == ['candidate', 'previous']
assert records[0]['certificate_sha256'] == records[1]['certificate_sha256']
def stats(r): return [s for s in r['solver_statistics'] if not s.startswith('c timing')]
assert stats(records[0]) == stats(records[1])
tasks = m['wave_tasks'][str(wave)]
p = c / f'wave{wave}-submission.json'
assert not p.exists(), 'Do not replace an existing submission record'
p.write_text(json.dumps(dict(job=job, tasks=tasks,
    command=f'sbatch --parsable --array={tasks[0]}-{tasks[1]} {c.relative_to(root)}/array.sbatch',
    recorded_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    manifest_sha256=sha(c / 'manifest.json'), predecessor_solver_snapshots=evidence,
    validation='Expected UNSAT exits, complete non-timing statistics and proof bytes match; certificates remain asynchronous and unverified until checked.',
    test_consideration='Reuse the campaign strict identity contract; this records submission provenance and does not change solver or runner behavior.'), indent=2) + '\n')
print(p)
