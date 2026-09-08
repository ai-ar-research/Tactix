#!/usr/bin/env python3
"""Validate observer replay before comparing the complete last-common heap."""
import csv
import hashlib
import itertools
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5883-rank32-choice-frontier'
target = c / 'choice-validation.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
bindings = {str((c / 'manifest.json').relative_to(root)): sha(c / 'manifest.json')}
heaps, choices = {}, {}
for arm in ('candidate', 'kissat'):
    p = c / 'results' / (arm + '.json')
    r = json.loads(p.read_text())
    binding = m['cached_controls'][arm]
    q = root / binding['record']
    assert sha(q) == binding['sha256']
    old = json.loads(q.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
    assert r['partial_certificate'] == old['partial_certificate']
    for name, digest in r['trace_sha256'].items():
        assert sha(c / 'traces' / name) == digest
        if name in old['trace_sha256']: assert digest == old['trace_sha256'][name], name
    assert set(old['trace_sha256']).issubset(r['trace_sha256'])
    bindings[str(p.relative_to(root))] = sha(p)
    bindings[str(q.relative_to(root))] = sha(q)
    lines = (c / 'traces' / (arm + '.heap.tsv')).read_text().splitlines()
    assert lines[0] == '# ' + ('tactix' if arm == 'candidate' else arm) + '-heap-set\t1'
    assert lines[1] == '# external\tposition\tscore\tassigned\tactive'
    rows = [line.split('\t') for line in lines[2:]]
    assert all(len(row) == 5 and int(row[1]) == i for i, row in enumerate(rows))
    assert len({row[0] for row in rows}) == len(rows)
    heaps[arm] = {int(row[0]): dict(position=int(row[1]), score=row[2], assigned=int(row[3]), active=int(row[4])) for row in rows}
    choices[arm] = [line.split('\t') for line in (c / 'traces' / (arm + '.choice.tsv')).read_text().splitlines()]
    assert all(len(row) == 18 and row[0] == '225987' and row[4] == '116746' for row in choices[arm])
    assert [int(row[1]) for row in choices[arm]] == sorted(set(int(row[1]) for row in choices[arm]))
left, right = heaps['candidate'], heaps['kissat']
diffs = [dict(external=v, candidate=left.get(v), kissat=right.get(v)) for v in sorted(set(left) | set(right)) if left.get(v) != right.get(v)]
first = next((dict(index=i, candidate=a, kissat=b) for i, (a, b) in enumerate(itertools.zip_longest(choices['candidate'], choices['kissat'])) if a != b), None)
out = dict(observer_neutrality='Both arms replay every cached ordinary trace hash and partial proof byte through300K;no final proof or runtime claim.',
    heap_sizes={arm: len(rows) for arm, rows in heaps.items()}, heap_differences=diffs,
    first_choice_state_difference=first, hashes=bindings,
    limitation='Heap snapshot follows decision61017804 before subsequent propagation. Choice fields use portable reasons but internal missing-position sentinels may differ; classify fields,not raw rows alone.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps({k: v if k != 'heap_differences' else dict(count=len(v), first=v[:4]) for k, v in out.items() if k != 'hashes'}, indent=2))
