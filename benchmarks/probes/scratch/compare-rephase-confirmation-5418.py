#!/usr/bin/env python3
"""Require the repaired implications, complete heap and formerly split prefix."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5418-rank35-rephase-confirmation'
m = json.loads((c / 'manifest.json').read_text())
old = root / 'benchmarks/runs/20260907-5400-window-validation'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p): return [s.split('\t') for s in p.read_text().splitlines() if not s.startswith('#')]
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
heaps, events, prefixes = {}, {}, {}
for arm in ('candidate', 'kissat'):
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
    prefixes[arm] = rows(c / 'traces' / (arm + '.prefix.tsv'))[0][1:]
    heap = [r[:4] for r in rows(c / 'traces' / (arm + '.heap.tsv'))]
    assert heap and len({r[0] for r in heap}) == len(heap)
    assert all(int(r[1]) == i for i, r in enumerate(heap))
    assert all(float(heap[(i-1)//2][2]) >= float(r[2]) for i, r in enumerate(heap) if i)
    heaps[arm] = heap
    events[arm] = [r[3:5] + [r[6], r[7], r[-1]] for r in rows(c / 'traces' / (arm + '.events.tsv')) if r[2] == 'imply']
assert prefixes['kissat'] == rows(old / 'traces/kissat.prefix.tsv')[0][1:]
assert prefixes['candidate'] == prefixes['kissat']
assert sha(c / 'traces/candidate.decisions.tsv') == sha(c / 'traces/kissat.decisions.tsv')
assert heaps['candidate'] == heaps['kissat'] and len(heaps['candidate']) == 429257
assert events['candidate'] == events['kissat'] and len(events['candidate']) == 6
result = dict(clean_stops=2, native_prefix_identical_to_5400=True,
              complete_prefix_identical=prefixes['candidate'], window_decisions_identical=True,
              post_warmup_heap=dict(conflict=13563, decisions=17798664, size=429257, membership_scores_assignments_positions_identical=True, valid=True),
              selected_implication_rows_identical=events['candidate'],
              closed_witness='Missing root closure before Rephase caused selected implication ordering, all 75 post-warm-up heap-position differences, and the first decision split at 13832. Full decision prefix now matches through 13833.',
              limitation='Stopped causal confirmation only. Full runtime, B28+C5 and consolidation fuzz remain open; no global acceptance or promotion.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
