#!/usr/bin/env python3
"""Compare ordered core rows, distinguishing live pose from clause identity."""
import hashlib
import json
from pathlib import Path
import re

C = Path(__file__).resolve().parent

def values(line):
    return [(int(lit), int(value)) for lit, value in re.findall(r'(-?\d+):(-?\d+),', line)]

def source(line):
    return bool(int(re.search(r'learned=(\d)', line)[1]))

def unit(line):
    row = values(line)
    if any(v > 0 for _, v in row):
        return None
    live = [lit for lit, v in row if not v]
    return live[0] if len(live) == 1 else None

def signature(line, normalize):
    lits = tuple(lit for lit, _ in values(line))
    return source(line), tuple(sorted(lits)) if normalize else lits

t, k = {}, {}
q = None
for line in (C / 'logs/tactix.out').read_text().splitlines():
    if line.startswith('OBS_QUERY '):
        q = int(re.search(r'ordinal=(\d+)', line)[1])
    elif line.startswith('OBS_ORDER '):
        t.setdefault(q, []).append(line)
for line in (C / 'logs/kissat.out').read_text().splitlines():
    if line.startswith('OBS_SAVE '):
        q = int(re.search(r'query=(\d+)', line)[1])
        k.setdefault(q, []).append(line)
assert t and k
common = sorted(t.keys() & k.keys())
differences = {}
for mode in ('sets', 'pose'):
    mismatches = []
    for q in common:
        a = [signature(r, mode == 'sets') for r in t[q]]
        b = [signature(r, mode == 'sets') for r in k[q]]
        if a != b:
            first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            mismatches.append(dict(query=q, counts=[len(a), len(b)], first=first,
                                   tactix=a[first] if first < len(a) else None,
                                   kissat=b[first] if first < len(b) else None))
    differences[mode] = mismatches
hashes = {}
for arm in ('tactix', 'kissat'):
    data = (C / f'traces/{arm}.tsv').read_bytes()
    earlier = (C.parent / f'20260907-5313-core-boundary-selector/traces/{arm}.tsv').read_bytes()
    assert data == earlier, ('observer changed semantic trace', arm)
    hashes[arm] = hashlib.sha256(data).hexdigest()
print(json.dumps(dict(core_queries=dict(tactix=len(t), kissat=len(k), common=len(common)),
    absent_queries=dict(tactix_only=sorted(t.keys()-k.keys()), kissat_only=sorted(k.keys()-t.keys())),
    core_order_differences=differences,
    query20_units=dict(tactix=[unit(r) for r in t[20] if unit(r)],
                       kissat=[unit(r) for r in k[20] if unit(r)]),
    event_traces_unchanged=hashes,
    note='Core metadata observer only. No new units committed; physical pose and ordered clause identity are separate comparisons.'), indent=2))
