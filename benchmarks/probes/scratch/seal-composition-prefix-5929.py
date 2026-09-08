#!/usr/bin/env python3
"""Seal the negative composition result without altering earlier snapshots."""
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5925-reduce-preclean-rank32-prefix'
old = root / 'benchmarks/runs/20260908-5907-reduce-root-rank32-prefix'
target = c / 'parent-replay.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''): h.update(block)
    return h.hexdigest()
bindings = {}
records = []
for campaign in (old,c):
    p = campaign / 'results/candidate.json'
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert sha(campaign / 'pins/candidate') == r['binary_sha256']
    for name,digest in r['trace_sha256'].items(): assert sha(campaign / 'traces' / name) == digest
    bindings[str(p.relative_to(root))] = sha(p)
    records.append(r)
a,b = records
assert a['partial_certificate'] == b['partial_certificate']
assert a['trace_sha256'] == b['trace_sha256']
s = json.loads((c / 'summary.json').read_text())
assert s['matching_decisions'] == 68091135 and s['suffix_comparison']['left'][0] == 271085
bindings[str((c / 'summary.json').relative_to(root))] = sha(c / 'summary.json')
target.write_text(json.dumps(dict(hashes=bindings,partial_certificate=b['partial_certificate'],
    ordinary_trace_sha256=b['trace_sha256'],matching_native_decisions=s['matching_decisions'],
    outcome='Combined5920 replays parent5900 byte-for-byte through300K,including all four ordinary traces and partial proof. Preconnection composition does not close current Rank32 second-forward boundary or first choice split. This rejects that proposed local explanation,not every later possible effect.',
    next_action='Localize second-forward clause/partner work at271079. Continue full5926 andBC5924 to measure downstream effects;do not infer a composition speedup.'),indent=2)+'\n')
print('PASS: complete parent trace/proof replay;proposed local cleanup explanation rejected')
