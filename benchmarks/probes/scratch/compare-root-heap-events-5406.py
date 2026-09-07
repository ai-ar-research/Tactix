#!/usr/bin/env python3
"""Validate the targeted lifecycle witness without comparing local trail offsets."""
import hashlib
import json
from pathlib import Path
import re
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5406-rank35-root-heap-events'
m = json.loads((c / 'manifest.json').read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
events, prefixes = {}, {}
for arm in ('candidate','kissat'):
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
    prefix = next(l for l in (c / 'traces' / (arm + '.prefix.tsv')).read_text().splitlines() if not l.startswith('#')).split('\t')
    prefixes[arm] = prefix[1:]
    rows = []
    for line in (c / 'traces' / (arm + '.events.tsv')).read_text().splitlines():
        if line.startswith('#'): continue
        row = line.split('\t')
        if int(row[3]) < 13413: continue
        payload = re.sub(r'\|trail_after=\d+', '', row[-1])
        rows.append(dict(event=row[2], conflict=int(row[3]), decisions=int(row[4]), level=int(row[6]), mode=row[7], payload=payload))
    assert len(rows) == 3 and [r['event'] for r in rows] == ['heap_insert','root_assign','heap_remove']
    events[arm] = rows
assert prefixes['candidate'] == prefixes['kissat']
assert sha(c / 'traces/candidate.decisions.tsv') == sha(c / 'traces/kissat.decisions.tsv')
assert events['candidate'] == events['kissat']
result = dict(clean_stops=2, selected_event_count=3, portable_events_identical=True,
              prefix=prefixes['candidate'], selected_events=events['candidate'],
              next_action='The complete heap hash agrees after the selected reinsertion at conflict 13413, and the root removal pose agrees at 13414. Capture the last pre-Rephase heap and selected reinsertions through 13563.',
              limitation='Local trail offsets excluded; other event statistics have different scopes. This witness does not prove all intervening heap updates identical or provide a checked solve/timing result.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
