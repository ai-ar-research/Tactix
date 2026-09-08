#!/usr/bin/env python3
"""Bind the repaired Factor boundaries to the complete40-million-decision replay."""
from pathlib import Path
import hashlib
import json
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5597-factor-support-rank32-prefix'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
summary = json.loads((c / 'summary.json').read_text())
assert summary['clean_stops'] == 2 and summary['complete_prefix_identical']
assert summary['matching_rows'] == 40102204 and summary['first_decision_difference'] is None
def parse(content, name):
    rows = []
    for line in content.splitlines():
        if not line or line.startswith('#'): continue
        r = line.split('\t')
        assert len(r) in (13, 15)
        if r[2] == name and r[4] == '19375':
            assert r[3] in ('begin', 'end')
            rows.append([r[3], *map(int, (r[4], r[5], r[7], r[10], r[11], r[12]))])
    assert [r[0] for r in rows] == ['begin', 'end', 'begin', 'end']
    return rows
arms, evidence = {}, {}
for arm, name in [('candidate', 'Factor'), ('kissat', 'factor')]:
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    p = c / 'traces' / (arm + '.boundaries.tsv')
    assert sha(p) == r['trace_sha256'][p.name]
    evidence[arm] = sha(p)
    arms[arm] = parse(p.read_text(), name)
assert arms['candidate'] == arms['kissat']
assert arms['candidate'][-1][3:] == [161449, 552779, 2013, 203262]
result = dict(complete_prefix_identical=True, matching_decisions=40102204,
    parent_summary_sha256=sha(c / 'summary.json'), evidence=evidence,
    columns=['stage', 'conflict', 'decisions', 'active', 'irredundant_large', 'redundant_large', 'binary'],
    shared_boundaries=arms['candidate'],
    limitation='Boundary projections deliberately omit differently scoped propagation and root-trail counts. They supplement, not replace, whole decision/prefix validation and unchanged native parent proof replay. No full solve or runtime claim.')
p = c / 'factor-closure.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: both19375 Factor passes agree; second exit552779 irredundant large,203262 binary')
