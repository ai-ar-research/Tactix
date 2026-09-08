#!/usr/bin/env python3
"""Validate selected snapshots, distinguishing set differences from copies."""
import collections
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5437-rank35-sweep-binary-boundary'
arms = ('previous', 'candidate', 'kissat')
if any(not (c / 'results' / (a + '.json')).exists() for a in arms):
    print('5437 pending'); raise SystemExit(0)
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
snapshots, headers, prefixes = {}, {}, {}
for arm in arms:
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    assert r['binary_sha256'] == m['hashes']['pins/' + arm]
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
    events = (c / 'traces' / (arm + '.events.tsv')).read_text()
    assert 'trace_truncated' not in events and 'trace-truncated' not in events
    prefix = [line.split('\t')[1:] for line in (c / 'traces' / (arm + '.prefix.tsv')).read_text().splitlines() if not line.startswith('#')]
    assert len(prefix) == 1 and int(prefix[0][0]) == 19366
    assert prefix[0][5:8] == ['18516218', '1', 'a6eb1df4f4e0e591']
    prefixes[arm] = prefix[0]
    groups, meta, closed = {}, {}, set()
    for line in (c / 'traces' / (arm + '.binaries.tsv')).read_text().splitlines():
        row = line.split('\t')
        if row[0] == '# begin':
            index = int(row[1]); assert index not in groups
            assert int(row[4]) == 19365
            groups[index], meta[index] = collections.Counter(), row[2:]
        elif row[0] == '# end': closed.add(int(row[1]))
        elif not line.startswith('#'):
            index, a, b = map(int, row); assert a <= b
            groups[index][a, b] += 1
    assert len(groups) >= 2 and set(groups) == closed == set(range(len(groups)))
    assert all(sum(group.values()) == int(meta[index][3]) for index, group in groups.items())
    snapshots[arm], headers[arm] = groups, meta
comparisons = {}
for left, right in (('previous', 'kissat'), ('candidate', 'kissat'), ('previous', 'candidate')):
    a, b = snapshots[left], snapshots[right]
    assert set(a) == set(b), 'Snapshot admission differs; inspect raw event boundaries before ordinal alignment'
    comparisons[left + '_vs_' + right] = [dict(snapshot=i,
        left_header=headers[left][i], right_header=headers[right][i],
        equal_multiset=a[i] == b[i], equal_unique_set=set(a[i]) == set(b[i]),
        left_only=[[x, y, n] for (x, y), n in sorted((a[i]-b[i]).items())],
        right_only=[[x, y, n] for (x, y), n in sorted((b[i]-a[i]).items())]) for i in sorted(a)]
result = dict(clean_stops=3, prefixes=prefixes, comparisons=comparisons,
    limitation='Selected binary multisets distinguish duplicate counts from unique clauses. Formula digests and raw event boundaries remain required for causal interpretation; a count difference alone is not a semantic failure. No complete solve or timing acceptance.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
