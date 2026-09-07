#!/usr/bin/env python3
"""Validate complete standalone decision streams and locate the next split."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
streams, hashes = {}, {}
MASK = (1<<64)-1
for task,arm in enumerate(('tactix','kissat')):
    log = (C/'logs'/f'1668709_{task}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    raw = (C/'traces'/(arm+'.decisions.tsv')).read_bytes()
    rows = [tuple(map(int,line.split())) for line in raw.splitlines()]
    assert all(len(r)==5 and r[0]<=200000 for r in rows)
    p = [line.split('\t') for line in (C/'traces'/(arm+'.prefix.tsv')).read_text().splitlines() if not line.startswith('#')]
    assert len(p)==1 and p[0][1]=='200000' and int(p[0][4])==len(rows)
    digest = 1469598103934665603
    for r in rows:
        digest = ((digest ^ (r[3]&MASK))*1099511628211)&MASK
    assert f'{digest:016x}' == p[0][5]
    old = C.parent/'20260907-5343-round-retirement'/'traces'/(arm+'.tsv')
    previous = []
    for line in old.read_text().splitlines():
        if line.startswith('#'):
            continue
        r = line.split('\t')
        if r[2]=='decision' and r[3]=='75451':
            previous.append((int(r[3]),int(r[4]),int(r[6]),int(r[-1])))
    assert [r[:4] for r in rows if r[0]==75451] == previous
    streams[arm]=rows
    hashes[arm]=hashlib.sha256(raw).hexdigest()
def compare(a,b):
    i=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
    return dict(counts=[len(a),len(b)],matching_prefix=i,equal=a==b,
        first_tactix=a[i] if i<len(a) else None,first_kissat=b[i] if i<len(b) else None,
        context_tactix=a[max(0,i-3):i+4],context_kissat=b[max(0,i-3):i+4])
t,k=streams.values()
print(json.dumps(dict(stopped_clean=2,cumulative_prefix_hashes_valid=True,decisions_at_75451_unchanged=True,
    fields=['conflict','ordinary_decision_counter','level','external_literal','warming'],
    all_decisions=compare(t,k),ordinary=compare([r for r in t if not r[4]],[r for r in k if not r[4]]),
    trace_sha256=hashes,
    prefix_metadata_caveat='Legacy start_decisions subtracts warm-up-inclusive count from ordinary decisions and wraps unsigned; do not use it as an alignment key. Full emitted count and recomputed cumulative hash are validated instead.'),indent=2))
