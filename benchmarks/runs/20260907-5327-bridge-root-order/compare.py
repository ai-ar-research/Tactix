#!/usr/bin/env python3
"""Root literals are portable; trail offsets and speculative repeats are not."""
from collections import Counter
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
rows, hashes = [], {}
for arm in ('tactix', 'kissat'):
    path = C / 'traces' / f'{arm}.tsv'
    raw = path.read_bytes()
    assert b'# truncated' not in raw
    hashes[arm] = hashlib.sha256(raw).hexdigest()
    roots, last = [], 0
    for line in raw.decode().splitlines():
        if line.startswith('#'):
            continue
        fields = line.split('\t')
        last = max(last, int(fields[3]))
        if fields[2] == 'root_assign' and int(fields[3]) == 26913:
            roots.append(dict(p.split('=', 1) for p in fields[-1].split('|')))
    assert last >= 26914, (arm, last)
    rows.append(roots)
first = next((i for i, (a,b) in enumerate(zip(*rows)) if a['lit'] != b['lit']), None)
counter = [Counter(r['lit'] for r in a) for a in rows]
collapsed = []
for arm_rows in rows:
    kept = []
    for row in arm_rows:
        if kept and all(row[key] == kept[-1][key]
                        for key in ('lit', 'reason', 'trail_after')):
            continue
        kept.append(row)
    collapsed.append([(row['lit'], row['reason']) for row in kept])
print(json.dumps(dict(counts=list(map(len, rows)), first_mismatch=first,
                     mismatch_window=None if first is None else {
                         arm: rows[i][max(0,first-3):first+8]
                         for i,arm in enumerate(('tactix','kissat'))},
                     extra_records=dict(counter[0]-counter[1]),
                     missing_records=dict(counter[1]-counter[0]),
                     adjacent_identical_assignment_sensitivity=dict(
                         counts=list(map(len, collapsed)),
                         literals_and_reasons_equal=collapsed[0] == collapsed[1],
                         caveat='A sensitivity check only; does not prove whether the removed observer event was speculative.'),
                     caveat='Observer includes speculative root operations; repeated records are not automatically extra committed units.',
                     trace_sha256=hashes), indent=2))
