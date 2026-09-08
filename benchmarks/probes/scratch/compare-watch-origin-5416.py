#!/usr/bin/env python3
"""Validate Rephase-entry watch snapshots and reject an entry-pose hypothesis."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5416-rank35-watch-origin'
m = json.loads((c / 'manifest.json').read_text())
old = root / m['parent_campaign']
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
snapshots = []
for index, conflict in enumerate(m['snapshot_conflicts']):
    watches = {}
    for arm in ('candidate', 'kissat'):
        tag = f'{index:02d}-{arm}'
        r = json.loads((c / 'results' / (tag + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
        for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
        for suffix in ('prefix.tsv', 'decisions.tsv', 'events.tsv', 'boundaries.tsv'):
            assert sha(c / 'traces' / (tag + '.' + suffix)) == sha(old / 'traces' / (arm + '.' + suffix)), (tag, suffix)
        rows = [line.split('\t')[1:] for line in (c / 'traces' / (tag + '.watch.tsv')).read_text().splitlines() if not line.startswith('#')]
        assert rows and all(row[:3] == ['rephase_begin', str(conflict), '-145'] for row in rows)
        watches[arm] = rows
    t, k = watches['candidate'], watches['kissat']
    assert t == k
    target = [r for r in t if r[-2] == '-109796,-145,109797']
    snapshots.append(dict(conflict=conflict, matching_large_watch_rows=len(t), complete_emitted_fields_identical=True, target_reason_rows=target))
result = dict(clean_stops=6, unchanged_prefix_stream_events_boundaries=True,
              snapshots=snapshots,
              inference='The ternary reason does not yet watch -145 at Rephase entry. Investigate retained-root propagation after restart, before the first warm-up decision; do not change watch ordering based on the earlier hypothesis.',
              limitation='Macro observer emits large watches and their ordinal among all entries; it does not emit binary contents. No complete solve or timing claim.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
