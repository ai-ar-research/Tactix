#!/usr/bin/env python3
"""Validate unchanged trajectory and compare selected warm-up implications."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
number = int(sys.argv[1]) if len(sys.argv) > 1 else 5413
assert number in (5413, 5415)
c = root / f'benchmarks/runs/20260907-{number}-rank35-warmup-implications'
m = json.loads((c / 'manifest.json').read_text())
old = root / m['matched_prefix_authority']
if any(not (c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
    print(f'{number} pending'); raise SystemExit(0)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
events, boundary_notes = {}, {}
for arm in ('candidate', 'kissat'):
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
    for suffix in ('prefix.tsv', 'decisions.tsv'):
        assert sha(c / 'traces' / (arm + '.' + suffix)) == sha(old / 'traces' / (arm + '.' + suffix)), (arm, suffix)
    def boundaries(base):
        rows = [s.split('\t') for s in (base / 'traces' / (arm + '.boundaries.tsv')).read_text().splitlines() if not s.startswith('#')]
        return [r[2:] for r in rows if r[2] != 'analysis'], [r[2:] for r in rows if r[2] == 'analysis']
    coarse, analysis = boundaries(c)
    old_coarse, old_analysis = boundaries(old)
    assert coarse == old_coarse
    if arm == 'candidate': assert analysis == old_analysis == []
    else:
        assert old_analysis == [] and len(analysis) == 1
        assert analysis[0][:4] == ['analysis', 'end', '13563', '17798663']
    boundary_notes[arm] = dict(coarse_rows_identical=True, additional_analysis_rows=analysis)
    body = (c / 'traces' / (arm + '.events.tsv')).read_text()
    assert '# truncated' not in body
    events[arm] = [dict(conflict=int(r[3]), decisions=int(r[4]), level=int(r[6]), mode=r[7], payload=r[-1])
                   for line in body.splitlines() if not line.startswith('#') and (r := line.split('\t'))[2] == 'imply']
    assert events[arm]
t, k = events['candidate'], events['kissat']
result = dict(clean_stops=2, unchanged_prefix_stream=True, normalized_boundaries=boundary_notes,
              selected_event_counts={arm: len(es) for arm, es in events.items()}, selected_events=events,
              selected_events_identical=t == k,
              first_selected_difference=next((dict(index=i, candidate=a, kissat=b) for i, (a, b) in enumerate(zip(t, k)) if a != b), None),
              limitation='Selected implication stream, not all assignments. Context decisions/level are sampled after propagation in both observers. Same decisions do not establish same reasons/trail order. Stopped diagnostic, not checked final verdict or timing.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
