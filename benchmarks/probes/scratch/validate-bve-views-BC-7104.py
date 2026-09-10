#!/usr/bin/env python3
"""Require exact BVE cost-only work/certificates; permit asynchronous checkers."""
import csv
import datetime
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
reader = Path(__file__).with_name('validate-material-state-BC-6576.py')
spec = importlib.util.spec_from_file_location('bve_pair_control', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
h.fixtures()
if sys.argv[1:] == ['--self-test']: sys.exit(0)
assert sys.argv[1:] in ([], ['--seal'])
c = root / 'benchmarks/runs/20260910-7103-bve-views-BC-gate'
target = c / 'neutrality-7104.json'
if '--seal' in sys.argv: assert not target.exists()
hashes = {}
def bind(path, digest=None):
    actual = sha(path)
    assert digest is None or actual == digest, path
    hashes[str(path.relative_to(root))] = actual
def read(path):
    bind(path); return json.loads(path.read_text())
m = read(c / 'manifest.json'); launch = read(c / 'launch.json')
assert launch['manifest_sha256'] == sha(c / 'manifest.json')
for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
for rel in m['dependencies']:
    p = root / rel; gate = read(p)
    for path, digest in gate['hashes'].items(): bind(root / path, digest)
with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
assert len(matrix) == 66 and [int(x['task']) for x in matrix] == list(range(66))
records, states = {}, {}
for row in matrix:
    task = int(row['task'])
    for folder in ('provisional', 'results'):
        path = c / folder / f'{task:03d}.json'
        if not path.exists(): continue
        r = read(path); state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (path, state)
        assert r['task'] == task and r['rank'] == int(row['rank'])
        for key in ('arm', 'expected', 'instance'): assert r[key] == row[key], (path, key)
        if folder == 'results':
            assert state['checked'] and state['clean']
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        if task in records:
            h.compare_pair(records[task], r)
            assert records[task]['solve'] == r['solve']
        records[task], states[task] = r, state
pairs = []
for task in range(0, 66, 2):
    if task not in records or task + 1 not in records: continue
    a, b = records[task], records[task+1]
    assert a['arm'].startswith('current_') and b['arm'].startswith('candidate_')
    pairs.append(dict(rank=a['rank'], instance=a['instance'], expected=a['expected'],
        **h.compare_pair(a, b), current_seconds=a['solve']['wall_seconds'],
        candidate_seconds=b['solve']['wall_seconds']))
checked = sum(s['checked'] for s in states.values())
print(f'Published {len(records)}/66, exact pairs {len(pairs)}/33, checked/clean {checked}/66')
if '--seal' in sys.argv:
    assert len(records) == 66 and len(pairs) == 33
    bind(reader); bind(Path(__file__))
    with target.open('x') as stream:
        json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            hashes=hashes, solver_records=66, pairs=pairs, states=states,
            checked_clean=checked, all_work_and_certificates_equal=True,
            validation_pending=checked != 66,
            limitation='Exact reported non-timing statistics and certificates across all33 pairs. Not hidden-state identity or decision-grade runtime. Pending checkers permit provisional continuation, never promotion.'), stream, indent=2)
    print(target)
