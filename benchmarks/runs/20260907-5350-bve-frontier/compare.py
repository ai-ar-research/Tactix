#!/usr/bin/env python3
"""Compare symmetric begin/end BVE records; postflush coverage differs."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
streams, hashes = {}, {}
keys = ('external', 'stage', 'conflicts', 'ticks', 'limit', 'applied', 'value')
for task, arm in enumerate(('tactix', 'kissat')):
    log = (C / 'logs' / f'1668716_{task}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    for suffix in ('decisions.tsv', 'state.tsv', 'prefix.tsv', 'deact.tsv'):
        assert (C / 'traces' / (arm + '.' + suffix)).read_bytes() == (
            C.parent / '20260907-5349-choice-deactivation' / 'traces' / (arm + '.' + suffix)).read_bytes()
    raw = (C / 'traces' / (arm + '.bve.tsv')).read_bytes()
    rows = [dict(x.split('=', 1) for x in l.split()[1:])
            for l in raw.decode().splitlines() if not l.startswith('#')]
    rows = [r for r in rows if r['stage'] in ('begin', 'end')]
    assert len(rows) % 2 == 0
    for a, b in zip(rows[::2], rows[1::2]):
        assert a['stage'] == 'begin' and b['stage'] == 'end'
        assert a['external'] == b['external']
    streams[arm] = [{k: r[k] for k in keys} for r in rows]
    hashes[arm] = hashlib.sha256(raw).hexdigest()
a, b = streams.values()
i = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
assert i == 18 and a[i]['external'] == '4387' and b[i]['external'] == '16887'
print(json.dumps(dict(stopped_clean=2, previous_observers_byte_identical=True,
    symmetric_stage_policy='begin/end only; Kissat emits postflush after NoOp whereas Tactix does not',
    candidate_counts=[len(a)//2, len(b)//2], matching_complete_candidates=i//2,
    first_tactix=a[i], first_kissat=b[i],
    context_tactix=a[i-4:i+4], context_kissat=b[i-4:i+4], trace_sha256=hashes,
    conclusion='First nine candidates, outcomes and ticks match; tenth pop differs before effort diverges. Compare initial schedule before instrumenting dynamic heap updates.'), indent=2))
