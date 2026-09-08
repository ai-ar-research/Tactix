#!/usr/bin/env python3
"""Validate selected startup snapshot framing after full parent replay."""
import collections
import json
from pathlib import Path
import sys


def binary_snapshots(data, solver):
    groups, current = [], None
    for line in data.splitlines():
        row = line.split('\t')
        if row[0] == '# begin':
            assert current is None and len(row) == 7
            assert int(row[1]) == len(groups) and row[2] == solver
            assert int(row[4]) == 0 and 'stage=end' in row[6]
            current = dict(header=row[2:], clauses=[], count=int(row[5]))
        elif row[0] == '# end':
            assert current is not None and len(row) == 2
            assert int(row[1]) == len(groups)
            assert len(current['clauses']) == current['count']
            groups.append(current)
            current = None
        elif line and not line.startswith('#'):
            assert current is not None and len(row) == 3
            index, a, b = map(int, row)
            assert index == len(groups) and a <= b and a and b
            assert 43773 in (abs(a), abs(b))
            current['clauses'].append([a, b])
    assert current is None and groups
    return groups


def self_test():
    good = '# begin\t0\ttactix\tslot_boundary\t0\t2\tname=Factor|stage=end\n0\t3270\t43773\n0\t3270\t43773\n# end\t0\n'
    assert len(binary_snapshots(good, 'tactix')[0]['clauses']) == 2
    empty = good.replace('\t2\tname=', '\t0\tname=').replace('0\t3270\t43773\n', '')
    assert binary_snapshots(empty, 'tactix')[0]['clauses'] == []
    invalid = [good.replace('# end\t0\n', ''),
               good.replace('\t2\tname=', '\t1\tname='),
               good.replace('43773', '43774'),
               good.replace('# end\t0', '# end\t1'),
               good.replace('\tslot_boundary\t0\t', '\tslot_boundary\t1\t'),
               good.replace('stage=end', 'stage=begin')]
    for bad in invalid:
        try:
            binary_snapshots(bad, 'tactix')
        except AssertionError:
            pass
        else:
            raise AssertionError('Invalid snapshot accepted')
    print('PASS: two positive/six negative selected snapshot fixtures')


self_test()
if '--self-test' in sys.argv:
    raise SystemExit(0)
source = Path(__file__).with_name('compare-square-imply-5487.py').read_text()
source = source.replace('20260908-5487-square-implications',
                        '20260908-5496-square-definition').replace('5487 pending', '5496 pending')
exec(compile(source, __file__, 'exec'))
old = root / 'benchmarks/runs/20260908-5487-square-implications'
snapshots = {}
for arm, solver in (('candidate', 'tactix'), ('kissat', 'kissat')):
    for suffix in ('imply.tsv', 'decisions.tsv', 'prefix.tsv', 'boundaries.tsv'):
        assert sha(c / 'traces' / (arm + '.' + suffix)) == sha(old / 'traces' / (arm + '.' + suffix))
    groups = binary_snapshots((c / 'traces' / (arm + '.binary.tsv')).read_text(), solver)
    assert any(group['clauses'] for group in groups), 'Selected auxiliary not captured'
    formula = (c / 'traces' / (arm + '.formula.tsv')).read_text()
    assert formula.startswith('# ' + solver + '-formula-set\t2\n')
    large = []
    for line in formula.splitlines():
        if line.startswith('#'):
            continue
        row = line.split('\t')
        assert len(row) == 4 and int(row[0]) in (0, 1)
        lits = list(map(int, row[3].split(',')))
        assert len(lits) == int(row[1]) and 43773 in map(abs, lits)
        large.append(dict(redundant=int(row[0]), literals=lits))
    snapshots[arm] = dict(binary_boundaries=groups, factor_exit_large=large)
result = dict(parent_replay=True, snapshot_fixtures=8, snapshots=snapshots,
              limitation='Selected clause populations only; match named boundaries before comparison. No automatic semantic-equivalence, performance, or proof-acceptance claim.')
p = c / 'definition-snapshots.json'
if p.exists():
    assert json.loads(p.read_text()) == result
else:
    p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
