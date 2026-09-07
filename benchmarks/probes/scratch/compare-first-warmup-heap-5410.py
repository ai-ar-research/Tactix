#!/usr/bin/env python3
"""Validate the first warm-up heap against the preceding matched capture."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5410-rank35-first-warmup-heap'
m = json.loads((c / 'manifest.json').read_text())
old = root / m['parent_campaign']
if any(not (c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
    print('5410 pending'); raise SystemExit(0)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
heaps = {}
for arm in ('candidate', 'kissat'):
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
    for suffix in ('prefix.tsv', 'decisions.tsv', 'events.tsv', 'boundaries.tsv'):
        assert sha(c / 'traces' / (arm + '.' + suffix)) == sha(old / 'traces' / (arm + '.' + suffix))
    stream = (c / 'traces' / (arm + '.decisions.tsv')).read_text().splitlines()
    assert next(row for row in stream if row.startswith('13563\t')) == '13563\t17798663\t1\t145\t1'
    heap = [s.split('\t')[:4] for s in (c / 'traces' / (arm + '.heap.tsv')).read_text().splitlines() if not s.startswith('#')]
    assert heap and all(int(row[1]) == i for i, row in enumerate(heap))
    assert len({row[0] for row in heap}) == len(heap)
    assert all(float(heap[(i - 1)//2][2]) >= float(row[2]) for i, row in enumerate(heap) if i)
    heaps[arm] = heap
t, k = heaps['candidate'], heaps['kissat']
tm, km = ({row[0]: row for row in heap} for heap in (t, k))
assert tm.keys() == km.keys()
diffs = {field: [dict(variable=v, candidate=tm[v], kissat=km[v]) for v in tm if tm[v][index] != km[v][index]]
         for field, index in [('position', 1), ('score', 2), ('assignment', 3)]}
result = dict(clean_stops=2, unchanged_prefix_stream_events_boundaries=True,
              snapshot=dict(conflict=13563, ordinary_decisions=17798663, level=1, literal=145, warming=True),
              heap_size=len(t), identical_membership=True, valid_heaps=True,
              difference_counts={field: len(ds) for field, ds in diffs.items()},
              first_differences={field: ds[:5] for field, ds in diffs.items()},
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()},
              limitation='Stopped diagnostic, not checked final solve or timing. Snapshot active column excluded because representations differ.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
