#!/usr/bin/env python3
"""Neutral deactivation tracing and first differing inheap removal."""
import hashlib
import json
from pathlib import Path

C=Path(__file__).resolve().parent
streams,selected,hashes={},{},{}
for task,arm in enumerate(('tactix','kissat')):
    log=(C/'logs'/f'1668714_{task}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    for suffix in ('decisions.tsv','state.tsv','prefix.tsv'):
        assert (C/'traces'/(arm+'.'+suffix)).read_bytes()==(C.parent/'20260907-5348-choice-state'/'traces'/(arm+'.'+suffix)).read_bytes()
    raw=(C/'traces'/(arm+'.deact.tsv')).read_bytes()
    rows=[l.split() for l in raw.decode().splitlines()]
    streams[arm]=[r[:6] for r in rows if r[5]=='inheap=1']
    selected[arm]=[r for r in rows if r[2]=='5416']
    hashes[arm]=hashlib.sha256(raw).hexdigest()
a,b=streams.values()
i=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
print(json.dumps(dict(stopped_clean=2,previous_observers_byte_identical=True,
    inheap_counts=[len(a),len(b)],matching_prefix=i,first_tactix=a[i],first_kissat=b[i],
    context_tactix=a[i-3:i+4],context_kissat=b[i-3:i+4],selected_5416=selected,
    trace_sha256=hashes,conclusion='5416 is retired only by Tactix at conflict 95799. The first inheap difference is earlier in that phase, after 1554 matching removals: Kissat removes 22171 before Tactix next removes 3166. Trace the first BVE candidate difference.'),indent=2))
