#!/usr/bin/env python3
"""Full initial heap comparison and source-formula relevance inversion."""
import hashlib
import json
from pathlib import Path
import struct

C = Path(__file__).resolve().parent
headers, members, hashes = {}, {}, {}
for task, arm in enumerate(('tactix', 'kissat')):
    log = (C / 'logs' / f'1668718_{task}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    raw = (C / 'traces' / (arm + '.schedule.tsv')).read_bytes()
    lines = raw.decode().splitlines()
    headers[arm] = dict(x.split('=') for x in lines[1].split('\t')[1].split('|'))
    rows = [dict(x.split('=') for x in l.split()[1:])
            for l in lines if l.startswith('member\t')]
    assert len(rows) == int(headers[arm]['scheduled']) == 13003
    assert [int(r['position']) for r in rows] == list(range(len(rows)))
    for r in rows:
        score = struct.unpack('>d', bytes.fromhex(r['score']))[0]
        # Both pinned sources use eliminateocclim=2000. This recovers the
        # input relevance, not an observed stamp. Native mode.c identifies
        # the last '{' at 83003 as focused entry, whose relevance is stamp.
        pos, neg = min(int(r['positive']), 2000), min(int(r['negative']), 2000)
        r['decoded_score'] = score
        r['inferred_relevance'] = 2000**2 - (pos*neg-pos-neg) - score
    members[arm] = {r['external']: r for r in rows}
    assert len(members[arm]) == len(rows)
    hashes[arm] = hashlib.sha256(raw).hexdigest()
a, b = members.values()
assert a.keys() == b.keys()
common_headers = ('bound', 'scheduled', 'external_sum', 'set_xor', 'set_sum',
                  'insertion_hash', 'conflicts', 'decisions', 'active', 'irred', 'red', 'binary')
assert all(headers['tactix'][k] == headers['kissat'][k] for k in common_headers)
diffs = {k: sum(a[v][k] != b[v][k] for v in a)
         for k in ('positive', 'negative', 'score', 'position')}
assert diffs == dict(positive=0, negative=0, score=13003, position=262)
orders = {arm: sorted(rows, key=lambda v: rows[v]['inferred_relevance'])
          for arm, rows in members.items()}
assert orders['tactix'] == orders['kissat']
selected = {v: {arm: rows[v] for arm, rows in members.items()}
            for v in ('18548', '20303', '4387', '16887', '5416')}
print(json.dumps(dict(stopped_clean=2, headers=headers, differing_members=diffs,
    relevance_order_matches=13003, selected=selected, trace_sha256=hashes,
    source_formula='heap_score = 2000^2 - (min(pos,2000)*min(neg,2000)-min(pos,2000)-min(neg,2000)) - relevance',
    inference='Native focused mode uses VMTF stamps. Same relevance order but unequal numeric gaps point to stamping/compaction lifecycle; direct producer tracing remains required.',
    conclusion='Initial membership, occurrence counts and insertion hash match. Scores and 262 heap positions already differ before any candidate. Do not change BVE dynamic updates to mask this upstream state.'), indent=2))
