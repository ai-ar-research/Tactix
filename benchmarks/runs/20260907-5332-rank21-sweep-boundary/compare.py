#!/usr/bin/env python3
"""Locate macro-state differences without equating implementation-local ids."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
rows, hashes, sweeps = {}, {}, {}
for arm in ('core', 'sequential', 'kissat'):
    raw = (C / 'traces' / (arm + '.tsv')).read_bytes()
    assert b'# truncated' not in raw
    hashes[arm] = hashlib.sha256(raw).hexdigest()
    rows[arm] = [line.split('\t') for line in raw.decode().splitlines()
                 if not line.startswith('#')]
    assert rows[arm]
    sweeps[arm] = []
    for row in rows[arm]:
        payload = dict(p.split('=', 1) for p in row[-1].split('|') if '=' in p)
        if payload.get('name') == 'Sweep' or row[2] in ('sweep_begin', 'sweep_end'):
            sweeps[arm].append(dict(event=row[2],conflict=int(row[3]),payload=payload))
a, b = rows['core'], rows['sequential']
first = next((dict(index=i, core=x, sequential=y)
              for i,(x,y) in enumerate(zip(a,b)) if x[2:] != y[2:]), None)
print(json.dumps(dict(row_counts={arm:len(r) for arm,r in rows.items()},
    first_core_sequential_difference=first,
    last_observed_conflict={arm:int(r[-1][3]) for arm,r in rows.items()},
    sweeps=sweeps, trace_sha256=hashes,
    caveat='Boundary-only filtering omits the terminal analysis event; identical observed snapshots do not prove identical intermediate clause pose or decision trajectory.'),indent=2))
