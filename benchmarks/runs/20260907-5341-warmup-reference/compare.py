#!/usr/bin/env python3
"""Require terminal bounded traces and compare the warm-up implication prefix."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
streams, hashes = {}, {}
for task, arm in enumerate(('tactix', 'kissat')):
    log = (C/'logs'/f'1668686_{task}.log').read_text()
    assert f'{arm} exit {0 if arm == "tactix" else -15}' in log
    assert 'scratch_deleted True' in log
    raw = (C/'traces'/f'{arm}.tsv').read_bytes()
    assert b'trace_truncated' not in raw and b'trace_limit' not in raw
    rows = [line.split('\t') for line in raw.decode().splitlines() if not line.startswith('#')]
    assert rows[-1][2] == 'analysis_end' and rows[-1][3] == '75452'
    old = (C.parent/'20260907-5340-warmup-implications'/'traces'/f'{arm}.tsv').read_text()
    old_rows = [line.split('\t') for line in old.splitlines() if not line.startswith('#')]
    decisions = lambda rs: [(r[3], r[4], r[6], r[-1]) for r in rs if r[2] == 'decision']
    assert decisions(rows) == decisions(old_rows)
    streams[arm] = [(r[2], r[6], r[-1]) for r in rows if r[3] == '75451' and r[7] == 'stable' and r[2] in ('decision', 'imply')]
    hashes[arm] = hashlib.sha256(raw).hexdigest()
a, b = streams.values()
first = next(i for i, (x,y) in enumerate(zip(a,b)) if x != y)
assert first == 1155 and a[first] == ('decision', '24', '-2733') and b[first] == ('decision', '24', '14194')
print(json.dumps(dict(stopped_clean=2, decisions_unchanged_from_5340=True,
    matching_prefix=first, prefix_decisions=sum(r[0]=='decision' for r in a[:first]),
    prefix_implications=sum(r[0]=='imply' for r in a[:first]),
    first_difference=dict(tactix=a[first],kissat=b[first]),
    stream_lengths={arm:len(s) for arm,s in streams.items()}, trace_sha256=hashes,
    conclusion='All 23 preceding warm-up decisions and 1132 ordered implied literals/reasons match. The first observed difference is the zero-score decision choice, not preceding warm-up propagation.'), indent=2))
