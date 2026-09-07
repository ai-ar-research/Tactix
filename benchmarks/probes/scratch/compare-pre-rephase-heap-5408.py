#!/usr/bin/env python3
"""Check the common-prefix pre-Rephase state and selected lifecycle events."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5408-rank35-pre-rephase-heap'
old = root / 'benchmarks/runs/20260907-5405-rank35-heap-lifecycle'
m = json.loads((c / 'manifest.json').read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p): return [s.split('\t') for s in p.read_text().splitlines() if not s.startswith('#')]
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
heaps, events, prefixes = {}, {}, {}
for arm in ('candidate', 'kissat'):
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
    prefix = rows(c / 'traces' / (arm + '.prefix.tsv'))[0][1:]
    assert prefix == rows(old / 'traces' / ('02-' + arm + '.prefix.tsv'))[0][1:]
    prefixes[arm] = prefix
    heap = rows(c / 'traces' / (arm + '.heap.tsv'))
    assert heap and all(int(row[1]) == i for i, row in enumerate(heap))
    assert len({row[0] for row in heap}) == len(heap)
    assert all(float(heap[(i - 1)//2][2]) >= float(row[2]) for i, row in enumerate(heap) if i)
    heaps[arm] = {row[0]: row[:4] for row in heap}
    events[arm] = [dict(event=row[2], conflict=int(row[3]), decisions=int(row[4]),
                        level=int(row[6]), mode=row[7],
                        payload=re.sub(r'\|trail_after=\d+', '', row[-1]))
                   for row in rows(c / 'traces' / (arm + '.events.tsv')) if int(row[3]) >= 13414]
t, k = heaps['candidate'], heaps['kissat']
assert t.keys() == k.keys()
diffs = {field: [dict(variable=v, candidate=t[v], kissat=k[v]) for v in t if t[v][index] != k[v][index]]
         for field, index in [('position', 1), ('score', 2), ('assignment', 3)]}
te, ke = events['candidate'], events['kissat']
first = next((dict(index=i, candidate=a, kissat=b) for i, (a, b) in enumerate(zip(te, ke)) if a != b), None)
result = dict(clean_stops=2, prefix_identical_to_5405=prefixes, heap_size=len(t),
              snapshot_conflict=m['snapshot_conflict'], snapshot_decisions=m['snapshot_decisions'],
              identical_membership=True, valid_heaps=True,
              difference_counts={field: len(ds) for field, ds in diffs.items()},
              first_differences={field: ds[:5] for field, ds in diffs.items()},
              selected_event_counts={arm: len(es) for arm, es in events.items()},
              first_selected_event_difference=first,
              selected_events_identical=te == ke,
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()},
              limitation='Stopped diagnostic, not a solve or timing result. Active columns and local trail offsets have different representations and are excluded. Selected events are not an exhaustive lifecycle trace.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
