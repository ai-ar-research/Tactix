#!/usr/bin/env python3
"""Require exact-parent observer neutrality before interpreting round exits."""
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5779-rank32-forward-round'
if (c / 'summary.json').exists():
    print('Already sealed')
    raise SystemExit(0)
if not (c / 'results/candidate.json').exists():
    print('5779 bounded solve pending')
    raise SystemExit(0)
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
r = json.loads((c / 'results/candidate.json').read_text())
assert r['exit_code'] == 0 and r['scratch_deleted']
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
for rel, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / rel) == digest
parent = root / m['parent_campaign']
old = json.loads((parent / 'results/candidate.json').read_text())
assert old['exit_code'] == 0 and old['scratch_deleted']
for rel, digest in old['trace_sha256'].items():
    assert sha(parent / 'traces' / rel) == digest
    assert r['trace_sha256'][rel] == digest
assert r['partial_certificate'] == old['partial_certificate']
assert set(r['trace_sha256']) - set(old['trace_sha256']) == {'candidate.forward-round.tsv'}
rounds = []
for line in (c / 'traces/candidate.forward-round.tsv').read_text().splitlines():
    stage, *fields = line.split()
    values = dict(field.split('=', 1) for field in fields)
    if stage == 'begin': rounds.append(dict(begin=values))
    else: rounds[-1][stage] = values
assert len(rounds) == 2
for row in rounds:
    end = row['end']
    assert end['interrupted'] == '0'
    reached, scheduled = int(end['reached']), int(end['candidates'])
    assert 0 <= reached <= scheduled
    if end['ended'] == '1':
        assert int(row['unit-stop']['index']) + 1 == reached
        assert int(row['unit-stop']['tail']) + reached == scheduled
        row['exit_reason'] = 'unit'
    elif reached < scheduled:
        assert int(end['spent']) > int(end['budget']) and row['begin']['bounded'] == '1'
        row['exit_reason'] = 'quota'
    else: row['exit_reason'] = 'complete stream'
out = dict(observer_parent_trace_and_proof_equal=True, stop_conflict=m['stop_conflict'],
           manifest_sha256=sha(c / 'manifest.json'), result_sha256=sha(c / 'results/candidate.json'),
           trace_sha256=r['trace_sha256'], rounds=rounds,
           limitation='Diagnostic only; matching decision/boundary streams and proof bytes establish observer neutrality. Does not establish the eight-clause difference cause or a performance repair.')
(c / 'summary.json').write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(out, indent=2))
