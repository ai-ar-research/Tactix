#!/usr/bin/env python3
"""Validate neutral tracing and locate the first changed heap deactivation."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
streams, hashes = {}, {}
for task, arm in enumerate(('tactix', 'kissat')):
    log = (C/'logs'/f'1668688_{task}.log').read_text()
    assert f'{arm} exit {0 if arm == "tactix" else -15}' in log and 'scratch_deleted True' in log
    raw = (C/'traces'/f'{arm}.tsv').read_bytes()
    assert b'trace_truncated' not in raw and b'trace_limit' not in raw
    rows = [l.split('\t') for l in raw.decode().splitlines() if not l.startswith('#')]
    assert rows[-1][2:4] == ['analysis_end', '75452']
    old = (C.parent/'20260907-5341-warmup-reference'/'traces'/f'{arm}.tsv').read_text()
    old_rows = [l.split('\t') for l in old.splitlines() if not l.startswith('#')]
    semantic = lambda rs: [(r[2],r[3],r[4],r[6],r[-1]) for r in rs if r[2] in ('decision','imply')]
    assert semantic(rows) == semantic(old_rows)
    deact = (C/'traces'/f'{arm}.deact.tsv').read_bytes()
    stream = []
    for line in deact.decode().splitlines():
        fields = line.split()
        assert fields[0] == 'DEACT'
        flags = dict(f.split('=') for f in fields[3:])
        stream.append(dict(conflict=int(fields[1]), external=int(fields[2]),
                           eliminated=int(flags['elim']), fixed=int(flags['fixed']), inheap=int(flags['inheap'])))
    streams[arm] = stream
    hashes[arm] = hashlib.sha256(deact).hexdigest()
def compare(a, b):
    i = next((i for i,(x,y) in enumerate(zip(a,b)) if x != y), min(len(a),len(b)))
    return dict(counts=[len(a),len(b)], matching_prefix=i,
                first_tactix=a[i] if i<len(a) else None,
                first_kissat=b[i] if i<len(b) else None,
                context_tactix=a[max(0,i-3):i+4],context_kissat=b[max(0,i-3):i+4])
a,b=streams.values()
print(json.dumps(dict(stopped_clean=2, decisions_implications_unchanged=True,
    all_deactivations=compare(a,b),
    inheap_deactivations=compare([r for r in a if r['inheap']],[r for r in b if r['inheap']]),
    trace_sha256=hashes),indent=2))
