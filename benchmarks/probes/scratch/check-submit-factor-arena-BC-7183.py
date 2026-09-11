#!/usr/bin/env python3
"""Read-only exact Factor-arena BC comparison, or submit its once-only array."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260910-7183-factor-arena-BC'
reader = Path(__file__).with_name('submit-vivify-compact-wave-7150.py')
spec = importlib.util.spec_from_file_location('exact_fingerprint', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
assert sys.argv[1:] in ([], ['--submit'])
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
for rel, digest in m['dependencies'].items(): assert sha(root / rel) == digest, rel
with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
assert len(matrix) == 66 and [int(r['task']) for r in matrix] == list(range(66))
records, states = {}, {}
for folder in ('provisional', 'results'):
    for p in sorted((c / folder).glob('*.json')):
        r = json.loads(p.read_text()); task = r['task']
        assert task == int(p.stem) and task in range(66)
        for key in ('arm', 'expected', 'instance'): assert r[key] == matrix[task][key]
        assert r['rank'] == int(matrix[task]['rank'])
        state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
        if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        if task in records:
            assert h.fingerprint(r) == h.fingerprint(records[task]) and r['solve'] == records[task]['solve']
        records[task], states[task] = r, state
pairs = 0
for task in range(0, 66, 2):
    if task in records and task+1 in records:
        assert h.fingerprint(records[task]) == h.fingerprint(records[task+1]), task
        pairs += 1
print('Factor arena BC:', len(records), '/66 expected;', pairs, '/33 exact pairs;', sum(s['checked'] for s in states.values()), 'checked-clean')
if not sys.argv[1:]: sys.exit(0)
assert not records and not (c / 'launch.json').exists(), 'already launched; monitor existing job'
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', str(c / 'array.sbatch')]
job = subprocess.check_output(command, text=True).strip().split(';')[0]; assert job.isdigit()
with (c / 'launch.json').open('x') as out:
    json.dump(dict(job_id=job, accounting_since=since, command=command,
        manifest_sha256=sha(c / 'manifest.json'), helper_sha256=sha(Path(__file__))), out, indent=2)
print(job, since)
