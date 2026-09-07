#!/usr/bin/env python3
"""Compare the retirement ablation against the unchanged native reference."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
streams, deactivations, hashes = {}, {}, {}
for task, arm in enumerate(('tactix','kissat')):
    log = (C/'logs'/f'1668690_{task}.log').read_text()
    assert f'{arm} exit {0 if arm == "tactix" else -15}' in log and 'scratch_deleted True' in log
    raw = (C/'traces'/f'{arm}.tsv').read_bytes()
    assert b'trace_truncated' not in raw and b'trace_limit' not in raw
    rows = [l.split('\t') for l in raw.decode().splitlines() if not l.startswith('#')]
    assert rows[-1][2:4] == ['analysis_end','75452']
    streams[arm] = [(r[2],r[6],r[-1]) for r in rows if r[3] == '75451' and r[7] == 'stable' and r[2] in ('decision','imply')]
    deact = (C/'traces'/f'{arm}.deact.tsv').read_bytes()
    events = []
    for line in deact.decode().splitlines():
        fields = line.split()
        flags = dict(f.split('=') for f in fields[3:])
        if flags['inheap'] == '1':
            events.append((int(fields[1]),int(fields[2]),int(flags['elim']),int(flags['fixed'])))
    deactivations[arm] = events
    hashes[arm] = dict(trace=hashlib.sha256(raw).hexdigest(),deact=hashlib.sha256(deact).hexdigest())
def compare(a,b):
    i = next((i for i,(x,y) in enumerate(zip(a,b)) if x != y),min(len(a),len(b)))
    return dict(counts=[len(a),len(b)],matching_prefix=i,equal=a==b,
        first_tactix=a[i] if i<len(a) else None,first_kissat=b[i] if i<len(b) else None)
print(json.dumps(dict(stopped_clean=2,inheap_deactivations=compare(*deactivations.values()),
    warmup=compare(*streams.values()),trace_sha256=hashes),indent=2))
