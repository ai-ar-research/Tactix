#!/usr/bin/env python3
"""Require each complete observed prefix to replay its longer parent trace."""
from pathlib import Path
import collections
import json

source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix',
                        '20260908-5490-rank32-sweep').replace('5436 pending', '5490 pending')
exec(compile(source, __file__, 'exec'))
old = root / m['parent_campaign']
assert sha(old / 'boundary-localization.json') == m['localization_sha256']
neutral = {}
for arm in ('candidate', 'kissat'):
    previous = json.loads((old / 'results' / (arm + '.json')).read_text())
    original = old / 'traces' / (arm + '.decisions.tsv')
    assert sha(original) == previous['trace_sha256'][original.name]
    before = rows(original)
    count = 0
    for row in rows(c / 'traces' / (arm + '.decisions.tsv')):
        assert next(before) == row, (arm, count)
        count += 1
    assert int(next(before)[0]) >= m['stop_conflict']
    assert count == int(prefixes[arm][3]) and count >= 18583703
    events = collections.Counter()
    for row in rows(c / 'traces' / (arm + '.sweep.tsv')):
        assert 'truncat' not in row[2].lower()
        if row[2] != 'trace_begin':
            assert int(row[3]) == 19375, row[:5]
        events[row[2]] += 1
    assert 0 < sum(events.values()) < 2000000
    assert events['sweep_schedule'] and events['sweep_seed']
    neutral[arm] = dict(replayed_rows=count, events=dict(events))
observation = dict(clean_stops=2, observer_neutral=neutral,
                   limitation='Prefix replay and selected observer validation, not semantic equality or a proved performance cause.')
p = c / 'observer-validation.json'
if p.exists():
    assert json.loads(p.read_text()) == observation
else:
    p.write_text(json.dumps(observation, indent=2) + '\n')
print(json.dumps(observation, indent=2))
