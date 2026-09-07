#!/usr/bin/env python3
"""Find portable decision and coarse boundary splits after three clean stops."""
import hashlib
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5396-rank27-prefix'
arms = ('previous', 'candidate', 'kissat')
if any(not (c / 'results' / (a + '.json')).exists() for a in arms):
    print('5396 pending'); raise SystemExit(0)
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['hashes'].items():
    assert sha(c / rel) == expected, rel
names = {'congruence': 'Congruence', 'substitute_early': 'Substitute',
         'substitute_late': 'Substitute', 'backbone_early': 'Backbone',
         'backbone_late': 'Backbone', 'vivify': 'Vivify', 'sweep': 'Sweep',
         'transitive': 'TransitiveReduce', 'factor': 'Factor',
         'strengthen': 'Strengthen', 'bve': 'BVE'}
boundaries, prefixes = {}, {}
for arm in arms:
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['partial_certificate']['bytes'] > 0
    assert r['binary_sha256'] == m['hashes']['pins/' + arm]
    for name, digest in r['trace_sha256'].items():
        assert sha(c / 'traces' / name) == digest
    prefixes[arm] = [l for l in (c / 'traces' / (arm + '.prefix.tsv')).read_text().splitlines()
                     if not l.startswith('#')]
    assert len(prefixes[arm]) == 1
    assert int(prefixes[arm][0].split('\t')[1]) == m['stop_conflict']
    rows = []
    for line in (c / 'traces' / (arm + '.boundaries.tsv')).read_text().splitlines():
        if line.startswith('#'):
            continue
        r = line.split('\t')
        if arm == 'kissat' and r[2] not in names:
            continue
        rows.append(dict(name=names.get(r[2], r[2]), stage=r[3], conflicts=int(r[4]),
                         decisions=int(r[5]), active=int(r[7]), irred=int(r[10]),
                         red=int(r[11]), binary=int(r[12])))
    boundaries[arm] = rows

comparisons = {}
for left, right in (('previous', 'kissat'), ('candidate', 'kissat'), ('previous', 'candidate')):
    with (c / 'traces' / (left + '.decisions.tsv')).open() as a, \
         (c / 'traces' / (right + '.decisions.tsv')).open() as b:
        first = None
        matched = 0
        for i, (x, y) in enumerate(itertools.zip_longest(a, b)):
            if x != y:
                first = dict(index=i, left=x.rstrip().split('\t') if x else None,
                             right=y.rstrip().split('\t') if y else None)
                break
            matched += 1
    a, b = boundaries[left], boundaries[right]
    first_boundary = next((dict(index=i, left=x, right=y)
                           for i, (x, y) in enumerate(itertools.zip_longest(a, b))
                           if x != y), None)
    comparisons[left + '_vs_' + right] = dict(matching_decision_rows=matched,
                                             first_decision=first,
                                             first_coarse_boundary=first_boundary)
result = dict(clean_stops=3, prefixes=prefixes, comparisons=comparisons,
              boundaries=boundaries,
              result_sha256={a: sha(c / 'results' / (a + '.json')) for a in arms},
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()},
              limitations='Coarse counts localize a boundary, not formula/state equality or weaker work. Stopped diagnostic, not acceptance timing/verdict. Ignore wrapped legacy start_decisions.')
p = c / 'summary.json'
if p.exists():
    assert json.loads(p.read_text()) == result
else:
    p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(comparisons, indent=2))
