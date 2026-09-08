#!/usr/bin/env python3
"""Compare actual per-seed costs without assuming the successor closes the gap."""
from pathlib import Path
import datetime
import itertools
import json
import runpy

root = Path(__file__).resolve().parents[3]
helpers = runpy.run_path(str(Path(__file__).with_name('align-sweep-effort-5520.py')))
helpers['fixtures']()
sha, read_calls = helpers['sha'], helpers['read_calls']
c = root / 'benchmarks/runs/20260908-5529-sweep-small-rank32-trace'
validation = c / 'repair-localization.json'
assert json.loads(validation.read_text())['clean_stops'] == 2
arms = {}
for arm in ('candidate', 'kissat'):
    path = c / 'traces' / f'{arm}.sweep.tsv'
    result = json.loads((c / 'results' / f'{arm}.json').read_text())
    assert sha(path) == result['trace_sha256'][path.name]
    arms[arm] = read_calls(path)
def projected(row):
    if row is None:
        return None
    env, effort = row['environment'], row['effort']
    return dict(seed=row['seed'], vars=int(env['vars']), clauses=int(env['clauses']),
                ticks=int(effort['delta']) if effort else 0,
                solves=int(effort['solves']) if effort else 0)
comparisons = []
for ordinal, (a, b) in enumerate(zip(arms['candidate'], arms['kissat'])):
    first = None
    for index, pair in enumerate(itertools.zip_longest(a, b)):
        x, y = map(projected, pair)
        if x != y:
            first = dict(index=index, candidate=x, kissat=y)
            break
    comparisons.append(dict(call=ordinal + 1,
        counts=dict(candidate=len(a), kissat=len(b)), first_difference=first,
        totals={arm: dict(ticks=sum(projected(row)['ticks'] for row in rows),
                         solves=sum(projected(row)['solves'] for row in rows))
                for arm, rows in [('candidate', a), ('kissat', b)]}))
record = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    repair_validation_sha256=sha(validation), calls=comparisons,
    limitation='Ordered seed, environment-size and actual per-seed cost equality only; does not prove identical clauses/models or complete execution after the stop.')
path = c / 'effort-localization.json'
assert not path.exists()
path.write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps(record, indent=2))
