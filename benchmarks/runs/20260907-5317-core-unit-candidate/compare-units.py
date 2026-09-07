#!/usr/bin/env python3
import json
from pathlib import Path

C = Path(__file__).resolve().parent
data = {}
for arm in ('tactix', 'kissat'):
    units, envs = [], []
    for line in (C / f'traces/{arm}.tsv').read_text().splitlines():
        if line.startswith('#'):
            assert 'truncated' not in line
            continue
        row = line.split('\t')
        if row[3] != '26913':
            continue
        p = dict(part.split('=', 1) for part in row[-1].split('|') if '=' in part)
        if row[2] == 'sweep_unit':
            units.append(int(p['lit']))
        if row[2] == 'sweep_environment':
            envs.append({key:p[key] for key in ('external', 'vars', 'clauses')})
    data[arm] = dict(units=units, environments=envs)
a, b = (data[arm]['units'] for arm in ('tactix', 'kissat'))
first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), None)
if first is None and len(a) != len(b):
    first = min(len(a), len(b))
print(json.dumps(dict(unit_counts=[len(a), len(b)], first_unit_mismatch=first,
    witness={arm:[x for x in data[arm]['units'] if abs(x) in (841,970,693,653,651,618,844)] for arm in data},
    all_units={arm:data[arm]['units'] for arm in data},
    environment_counts=[len(data[arm]['environments']) for arm in data],
    environment_sequence_equal=data['tactix']['environments'] == data['kissat']['environments']), indent=2))
