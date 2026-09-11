#!/usr/bin/env python3
"""Read-only telemetry campaign validation, or submit exactly one new wave."""
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
assert len(sys.argv) in (2, 4) and sys.argv[1] in ('bc', 'runtime')
bc = sys.argv[1] == 'bc'
c = root / ('benchmarks/runs/20260910-7174-runtime-telemetry-BC' if bc else
            'benchmarks/runs/20260910-7175-runtime-telemetry-runtime')
spec = importlib.util.spec_from_file_location('telemetry_contract', c / 'pins/telemetry_contract.py')
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
for rel, digest in m['dependencies'].items(): assert sha(root / rel) == digest, rel
with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
assert len(matrix) == (66 if bc else 18)
records, paths, states = {}, {}, {}
for folder in ('provisional', 'results'):
    for p in sorted((c / folder).glob('*.json')):
        r = json.loads(p.read_text()); task = r['task']
        assert task == int(p.stem) and task in range(len(matrix)), p
        row = matrix[task]
        for key in ('arm', 'instance', 'expected'): assert r[key] == row[key], (p, key)
        assert r['rank'] == int(row['rank']), p
        state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
        ablated = m['arms'][r['arm']]['telemetry_ablated']
        fp = h.fingerprint(r, ablated)
        if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        if task in records:
            assert fp == h.fingerprint(records[task], ablated) and r['solve'] == records[task]['solve'], p
        records[task], paths[task], states[task] = r, p, state
for task in range(0, len(matrix), 2):
    if task in records and task+1 in records:
        assert h.fingerprint(records[task]) == h.fingerprint(records[task+1]), task
if not bc:
    for task, r in records.items():
        oracle = m['previous_work_oracles'][str(r['rank'])]
        assert sha(root / oracle['path']) == oracle['sha256']
        assert h.fingerprint(r) == h.fingerprint(json.loads((root / oracle['path']).read_text())), task
        first = next((v for i,v in records.items() if i < 6 and v['arm'] == r['arm'] and v['rank'] == r['rank']), None)
        assert first and h.fingerprint(first) == h.fingerprint(r), task
print('Published', len(records), '/', len(matrix), 'exact outcomes; checked-clean', sum(s['checked'] for s in states.values()))
if len(sys.argv) == 2: sys.exit(0)
assert sys.argv[2] == '--submit'
wave = int(sys.argv[3]); assert wave == 0 if bc else wave in (0, 1, 2)
start, width = (0, 66) if bc else (6*wave, 6)
assert set(records) == set(range(start)), 'preceding solver wave incomplete or next wave already started'
target = c / ('launch.json' if bc else f'wave{wave}-submission.json')
assert not target.exists(), 'already submitted; monitor existing job'
since = dt.datetime.now().replace(microsecond=0).isoformat()
command = ['sbatch', '--parsable', f'--array={start}-{start+width-1}', str(c / 'array.sbatch')]
job = subprocess.check_output(command, text=True).strip().split(';')[0]
assert job.isdigit()
with target.open('x') as stream:
    json.dump(dict(job_id=job, accounting_since=since, tasks=list(range(start, start+width)),
        command=command, manifest_sha256=sha(c / 'manifest.json'), helper_sha256=sha(Path(__file__)),
        preceding_records={str(p.relative_to(root)): sha(p) for p in paths.values()}), stream, indent=2)
print(job, since)
