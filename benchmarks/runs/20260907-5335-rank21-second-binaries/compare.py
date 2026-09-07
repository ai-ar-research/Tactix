#!/usr/bin/env python3
"""Require checked solves and validate exact second-Sweep binary multiplicities."""
from collections import Counter
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
m = json.loads((C/'manifest.json').read_text())
streams, hashes = {}, {}
for task, arm in enumerate(('core', 'sequential')):
    r = json.loads((C/'results'/f'{task:03d}.json').read_text())
    assert r['task'] == task and r['arm'] == arm and r['rank'] == 21
    assert r['valid'] and r['solve']['exit_code'] == 10 and not r['solve']['timed_out']
    assert r['model_check']['result'] == 'verified' and r['model_check']['exit_code'] == 0
    assert r['artifact_deleted'] and not r['cleanup_pending']
    assert r['binary_sha256'] == m['arms'][arm]['sha256']
    raw = (C/'traces'/f'{arm}.binaries.tsv').read_bytes()
    lines = raw.decode().splitlines()
    begins = [line.split('\t') for line in lines if line.startswith('# begin')]
    ends = [line for line in lines if line.startswith('# end')]
    assert len(begins) == len(ends) == 1 and ends[0] == '# end\t0'
    assert begins[0][4] == '23889' and begins[0][6] == 'name=Sweep|stage=end'
    rows = [tuple(map(int, line.split('\t')[1:])) for line in lines if not line.startswith('#')]
    assert len(rows) == int(begins[0][5]) == (621085 if arm == 'core' else 621089)
    assert rows == sorted(rows) and all(len(row) == 2 for row in rows)
    boundary = (C/'traces'/f'{arm}.tsv').read_bytes()
    previous = (C.parent/'20260907-5333-rank21-standalone'/'traces'/f'{arm}.tsv').read_bytes()
    assert boundary == previous, 'Binary observer changed recorded trajectory'
    streams[arm] = Counter(rows)
    hashes[arm] = hashlib.sha256(raw).hexdigest()
def delta(a, b):
    return [dict(literals=list(pair), copies=n, core_copies=streams['core'][pair],
                 sequential_copies=streams['sequential'][pair]) for pair, n in sorted((a-b).items())]
print(json.dumps(dict(checked_clean=2, boundary_traces_identical_to_5333=True,
    binary_copies={arm:sum(c.values()) for arm,c in streams.items()},
    core_only=delta(streams['core'], streams['sequential']),
    sequential_only=delta(streams['sequential'], streams['core']),
    trace_sha256=hashes,
    caveat='Full binary multisets, not full formula or watch-order equality. No runtime or fuzz acceptance.'), indent=2))
