#!/usr/bin/env python3
"""Validate the observer and compare portable heap membership and pose."""
import hashlib
import itertools
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5403-rank35-direct-heap'
m = json.loads((c / 'manifest.json').read_text())
old = root / m['parent_campaign']
arms = ('candidate', 'kissat')
if any(not (c / 'results' / (a + '.json')).exists() for a in arms):
    print('5403 pending'); raise SystemExit(0)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
heaps = {}
for a in arms:
    r = json.loads((c / 'results' / (a + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name, digest in r['trace_sha256'].items():
        assert sha(c / 'traces' / name) == digest, name
    for suffix in ('decisions.tsv', 'prefix.tsv', 'boundaries.tsv', 'choice.tsv'):
        assert sha(c / 'traces' / (a + '.' + suffix)) == sha(old / 'traces' / (a + '.' + suffix)), (a, suffix)
    rows = [line.split('\t') for line in (c / 'traces' / (a + '.heap.tsv')).read_text().splitlines() if not line.startswith('#')]
    assert rows and all(int(row[1]) == i for i, row in enumerate(rows))
    assert len({r[0] for r in rows}) == len(rows)
    heaps[a] = rows
t, k = (heaps[a] for a in arms)
tm, km = ({r[0]: r for r in h} for h in (t, k))
first = next((dict(position=i, candidate=x, kissat=y) for i, (x, y) in enumerate(itertools.zip_longest(t, k)) if x != y), None)
common = tm.keys() & km.keys()
score_diffs = [dict(variable=v, candidate=tm[v], kissat=km[v]) for v in sorted(common, key=int) if tm[v][2] != km[v][2]]
value_diffs = [dict(variable=v, candidate=tm[v], kissat=km[v]) for v in sorted(common, key=int) if tm[v][3] != km[v][3]]
result = dict(clean_stops=2, prefix_and_window_identical_to_5400=True,
              snapshot=dict(conflict=m['snapshot_conflict'], decisions=m['snapshot_decisions']),
              heap_sizes={a: len(h) for a, h in heaps.items()},
              candidate_only=[tm[v] for v in sorted(tm.keys() - km.keys(), key=int)],
              kissat_only=[km[v] for v in sorted(km.keys() - tm.keys(), key=int)],
              score_difference_count=len(score_diffs), first_score_differences=score_diffs[:10],
              assignment_difference_count=len(value_diffs), first_assignment_differences=value_diffs[:10],
              first_positional_difference=first,
              selected_variables={v: {a: next((r for r in h if r[0] == v), None) for a, h in heaps.items()} for v in ('483970', '390547')},
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()},
              limitation='Diagnostic state, not a complete solve or runtime gate. Snapshot active columns use different retired/fixed representations and are not a semantic equality predicate.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
