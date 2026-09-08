#!/usr/bin/env python3
"""Compare startup Factor extension records after complete parent replay."""
import itertools
import json
from pathlib import Path
import sys


def payload(text, ordinal):
    fields = text.split('|')
    d = dict(field.split('=', 1) for field in fields)
    assert len(fields) == len(d) == 7
    assert set(d) == {'fresh', 'ordinal', 'seed', 'factors', 'remainders', 'delta', 'members'}
    result = {k: int(v) for k, v in d.items() if k != 'members'}
    result['members'] = list(map(int, d['members'].split(',')))
    assert result['ordinal'] == ordinal and result['fresh'] == 42387 + ordinal
    assert result['factors'] == len(result['members']) >= 2
    assert result['members'][0] == result['seed']
    assert result['remainders'] > 0 and result['delta'] > 0
    return result


def self_test():
    good = 'fresh=42388|ordinal=1|seed=-5720|factors=2|remainders=66|delta=64|members=-5720,609'
    assert payload(good, 1)['members'] == [-5720, 609]
    assert payload(good.replace('42388', '42389').replace('ordinal=1', 'ordinal=2'), 2)['ordinal'] == 2
    for bad in (good.replace('ordinal=1', 'ordinal=2'), good.replace('factors=2', 'factors=3'),
                good.replace('fresh=42388', 'fresh=42389'), good.replace('seed=-5720', 'seed=609'),
                good.replace('delta=64', 'delta=0'), good + '|delta=64'):
        try:
            payload(bad, 1)
        except AssertionError:
            pass
        else:
            raise AssertionError('Bad Factor record accepted')
    print('PASS: two positive/six negative Factor record fixtures')


self_test()
if '--self-test' in sys.argv:
    raise SystemExit(0)
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix',
                        '20260908-5502-square-factor-fresh').replace('5436 pending', '5502 pending')
exec(compile(source, __file__, 'exec'))
old = root / m['parent_campaign']
assert sha(old / 'summary.json') == m['prior_summary_sha256']
assert result == json.loads((old / 'summary.json').read_text())
events = {}
for arm in ('candidate', 'kissat'):
    for suffix in ('decisions.tsv', 'prefix.tsv', 'boundaries.tsv'):
        assert sha(c / 'traces' / (arm + '.' + suffix)) == sha(old / 'traces' / (arm + '.' + suffix))
    data = (c / 'traces' / (arm + '.factor.tsv')).read_text()
    assert 'truncat' not in data.lower()
    captured = []
    for sequence, row in enumerate(rows(c / 'traces' / (arm + '.factor.tsv'))):
        assert len(row) == 22 and int(row[1]) == sequence
        if row[2] == 'trace_begin':
            assert sequence == 0
            continue
        assert row[2] == 'factor_fresh' and int(row[3]) == 0
        captured.append(payload(row[-1], len(captured) + 1))
    assert len(captured) >= 1386 and len(captured) < 100000
    events[arm] = captured
first = next((dict(ordinal=i + 1, candidate=a, kissat=b)
              for i, (a, b) in enumerate(itertools.zip_longest(events['candidate'], events['kissat']))
              if a != b), None)
result = dict(parent_replay=True, fixtures=8, counts={a: len(v) for a, v in events.items()},
              first_extension_difference=first,
              limitation='Accepted seed/member grids identify a boundary, not equality of the incoming physical formula or an established performance cause.')
p = c / 'factor-localization.json'
if p.exists():
    assert json.loads(p.read_text()) == result
else:
    p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
