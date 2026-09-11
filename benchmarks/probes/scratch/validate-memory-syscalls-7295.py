#!/usr/bin/env python3
"""Validate whole exact Release work before interpreting external syscall traces."""
import csv
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
path = Path(__file__).with_name('occurrence-fingerprint-7256.py')
spec = importlib.util.spec_from_file_location('fingerprint', path)
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
h.fixtures()
assert sys.argv[1:] in ([], ['--self-test'], ['--seal'])
if '--self-test' in sys.argv:
    raise SystemExit(0)
c = root / 'benchmarks/runs/20260911-7295-memory-syscalls-rank31'
target = c / 'summary.json'
if '--seal' in sys.argv:
    assert not target.exists(), 'successful summary frozen'
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
bind(c / 'manifest.json')
bind(c / 'launch.json')
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    bind(c / rel, digest)
for rel, digest in m['evidence_sha256'].items():
    bind(root / rel, digest)
with (c / 'matrix.tsv').open() as stream:
    matrix = list(csv.DictReader(stream, delimiter='\t'))
assert len(matrix) == 2 and [r['arm'] for r in matrix] == ['candidate', 'previous']
records, states = {}, {}
for folder in ('provisional', 'results'):
    for p in sorted((c / folder).glob('*.json')):
        r = json.loads(p.read_text())
        bind(p)
        task = r['task']
        assert task == int(p.stem) and task in (0, 1) and r['rank'] == 31
        for key in ('arm', 'expected', 'instance'):
            assert r[key] == matrix[task][key]
        state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_UNSAT'), (p, state)
        if folder == 'results':
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        if task in records:
            assert h.raw_fingerprint(r) == h.raw_fingerprint(records[task])
            assert r['solve'] == records[task]['solve']
        oracle = m['release_records'][r['arm']]
        q = root / oracle['path']
        bind(q, oracle['sha256'])
        assert h.raw_fingerprint(r) == h.raw_fingerprint(json.loads(q.read_text()))
        trace = r['solve']['memory_syscalls']
        log = c / f'logs/{task:03d}.memory-syscalls.txt'
        assert trace['path'] == str(log.relative_to(c)) and trace['bytes'] == log.stat().st_size
        assert trace['bytes'] > 0
        bind(log, trace['sha256'])
        records[task], states[task] = r, state
bind(Path(__file__))
bind(path)
print('External Rank31:', len(records), '/2 exact outcomes;', sum(s['checked'] for s in states.values()), 'checked-clean')
for task, r in sorted(records.items()):
    print(r['arm'], r['solve']['wall_seconds'], r['solve']['peak_rss_kb'], r['solve']['memory_syscalls'])
if '--seal' in sys.argv:
    assert len(records) == 2
    with target.open('x') as out:
        json.dump(dict(hashes=hashes, records=records, states=states,
            limitation=m['limitation']), out, indent=2)
    print(target)
