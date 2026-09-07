#!/usr/bin/env python3
"""Validate all declared endpoints, then bracket physical heap divergence."""
import hashlib
import itertools
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5404-rank35-heap-bracket'
m = json.loads((c / 'manifest.json').read_text())
if len(list((c / 'results').glob('*.json'))) != 12:
    print('5404 pending'); raise SystemExit(0)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
points = []
for i, point in enumerate(m['points']):
    heaps, stream_hashes = {}, {}
    for arm in ('candidate', 'kissat'):
        label = f'{i:02d}-' + arm
        r = json.loads((c / 'results' / (label + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
        for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest, name
        row = next(l for l in (c / 'traces' / (label + '.prefix.tsv')).read_text().splitlines() if not l.startswith('#')).split('\t')
        assert int(row[1]) == point['snapshot_conflict'] and int(row[4]) == point['expected_count']
        assert row[5] == point['expected_hash'] and row[8] == point['common_hash']
        rows = [line.split('\t') for line in (c / 'traces' / (label + '.heap.tsv')).read_text().splitlines() if not line.startswith('#')]
        assert rows and all(int(r[1]) == j for j, r in enumerate(rows))
        assert len(rows) == len({r[0] for r in rows})
        heaps[arm] = rows
        stream_hashes[arm] = sha(c / 'traces' / (label + '.decisions.tsv'))
    assert stream_hashes['candidate'] == stream_hashes['kissat']
    t, k = heaps['candidate'], heaps['kissat']
    tm, km = ({r[0]: r for r in h} for h in (t, k))
    common = tm.keys() & km.keys()
    diffs = [dict(position=j, candidate=x, kissat=y) for j, (x, y) in enumerate(itertools.zip_longest(t, k)) if (x[:4] if x else x) != (y[:4] if y else y)]
    points.append(dict(point=point, heap_sizes={a: len(h) for a, h in heaps.items()},
                       membership_identical=tm.keys() == km.keys(),
                       score_differences=sum(tm[v][2] != km[v][2] for v in common),
                       assignment_differences=sum(tm[v][3] != km[v][3] for v in common),
                       positional_differences=len(diffs), first_differences=diffs[:3],
                       candidate_only=[tm[v] for v in sorted(tm.keys() - km.keys(), key=int)],
                       kissat_only=[km[v] for v in sorted(km.keys() - tm.keys(), key=int)],
                       heap_invariant_violations={a: sum(float(h[(j-1)//2][2]) < float(r[2]) for j, r in enumerate(h) if j) for a, h in heaps.items()}))
result = dict(clean_stops=12, all_prefixes_match_retained_common_5398=True, points=points,
              limitation='Bracket over selected post-decision stable-mode states, not proof of identity between checkpoints or a runtime/solve gate.',
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()})
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(points, indent=2))
