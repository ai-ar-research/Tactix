#!/usr/bin/env python3
"""Compare complete bounded decision streams and retain the next frontier."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
streams, hashes = {}, {}
for task,arm in enumerate(('tactix','kissat')):
    log = (C/'logs'/f'1668707_{task}.log').read_text()
    assert f'{arm} exit {0 if arm == "tactix" else -15}' in log and 'scratch_deleted True' in log
    path = C/'traces'/(arm+'.tsv')
    digest = hashlib.sha256()
    stream = []
    warming = False
    last = None
    with path.open('rb') as source:
        for raw in source:
            digest.update(raw)
            assert b'truncated' not in raw and b'trace_limit' not in raw
            if raw.startswith(b'#'):
                continue
            r = raw.decode().rstrip('\n').split('\t')
            last = r
            if r[2] == 'rephase_begin':
                warming = True
            elif r[2] == 'rephase_end':
                warming = False
            elif r[2] == 'decision' and 50000 <= int(r[3]) <= 200000:
                stream.append(dict(conflict=int(r[3]),counter=int(r[4]),level=int(r[6]),lit=int(r[-1]),warming=warming))
    assert last[2:4] == ['analysis_end','200001']
    streams[arm] = stream
    hashes[arm] = digest.hexdigest()
def compare(a,b):
    i = next((i for i,(x,y) in enumerate(zip(a,b)) if x != y),min(len(a),len(b)))
    return dict(counts=[len(a),len(b)],matching_prefix=i,equal=a==b,
        first_tactix=a[i] if i<len(a) else None,first_kissat=b[i] if i<len(b) else None,
        context_tactix=a[max(0,i-3):i+3],context_kissat=b[max(0,i-3):i+3])
t,k=streams.values()
print(json.dumps(dict(stopped_clean=2,combined=compare(t,k),
    ordinary=compare([r for r in t if not r['warming']],[r for r in k if not r['warming']]),
    trace_sha256=hashes),indent=2))
