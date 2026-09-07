#!/usr/bin/env python3
"""Validate complete stopped exports, including the selected boundary count."""
from collections import Counter
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
streams, hashes = {}, {}
for task, arm in enumerate(('core', 'sequential')):
    log = (C/'logs'/f'1668674_{task}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    raw = (C/'traces'/f'{arm}.tsv').read_bytes()
    lines = raw.decode().splitlines()
    begins = [line.split('\t') for line in lines if line.startswith('# begin')]
    ends = [line for line in lines if line.startswith('# end')]
    assert len(begins) == len(ends) == 1 and ends[0] == '# end\t0'
    assert begins[0][4] == '23889' and begins[0][6] == 'name=Sweep|stage=end'
    rows = [tuple(map(int, line.split('\t')[1:])) for line in lines if not line.startswith('#')]
    assert len(rows) == int(begins[0][5]) == 1007634
    assert rows == sorted(rows) and all(len(row) == 2 for row in rows)
    streams[arm] = Counter(rows)
    hashes[arm] = hashlib.sha256(raw).hexdigest()
assert streams['core'] == streams['sequential']
print(json.dumps(dict(stopped_clean=2, binary_copies=1007634, multiset_equal=True,
    trace_sha256=hashes, target_accepted=False,
    reason='Selector captured the first of two Sweeps at conflict 23889. The changed boundary is the second Sweep; 5335 selects ordinal 1 without SET_STOP.'), indent=2))
