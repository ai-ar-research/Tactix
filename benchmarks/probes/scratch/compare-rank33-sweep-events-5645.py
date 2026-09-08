#!/usr/bin/env python3
"""Validate full parent replay and localize selected Sweep work differences."""
from collections import Counter
import itertools
import json
from pathlib import Path
import runpy
import sys
support = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root, sha = support['root'], support['sha']

def classify(arm, event, conflict):
    if conflict == 120330: return 'selected'
    assert arm == 'kissat' and 0 <= conflict <= 1000000
    assert event in ('sweep_schedule', 'sweep_seed', 'sweep_environment', 'sweep_effort')
    return 'outside_native'

def fixtures():
    assert classify('candidate', 'sweep_seed', 120330) == 'selected'
    assert classify('kissat', 'sweep_equivalence', 120330) == 'selected'
    for event in ('sweep_schedule', 'sweep_seed', 'sweep_environment', 'sweep_effort'):
        for at in (0, 120331): assert classify('kissat', event, at) == 'outside_native'
    for args in (('candidate', 'sweep_seed', 0), ('kissat', 'sweep_unit', 0),
                 ('kissat', 'sweep_equivalence', 120331), ('kissat', 'sweep_seed', -1)):
        try: classify(*args)
        except AssertionError: pass
        else: raise AssertionError('Unowned out-of-window event accepted')
    print('PASS: ten positive/four negative Sweep window fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260908-5644-rank33-sweep-events'
    if any(not (c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
        print('5644 pending'); return
    evidence = support['validate'](c)
    arms, counts, outside = {}, {}, {}
    for arm in ('candidate', 'kissat'):
        calls, current, sequence = [], None, 0
        counts[arm], outside[arm] = Counter(), Counter()
        for line in (c / 'traces' / (arm + '.sweep.tsv')).open():
            if line.startswith('#'): continue
            row = line.rstrip('\n').split('\t')
            assert len(row) == 22 and int(row[1]) == sequence
            sequence += 1
            event, data = row[2], row[-1]
            assert 'truncat' not in event.lower()
            if event == 'trace_begin': continue
            if classify(arm, event, int(row[3])) != 'selected':
                outside[arm][event] += 1; continue
            counts[arm][event] += 1
            begin = (event == 'slot_boundary' and data == 'name=Sweep|stage=begin') or event == 'sweep_begin'
            end = (event == 'slot_boundary' and data == 'name=Sweep|stage=end') or event == 'sweep_end'
            if begin:
                assert current is None
                current = {kind: [] for kind in ('sweep_seed', 'sweep_environment', 'sweep_effort', 'sweep_unit', 'sweep_equivalence')}
            elif end:
                assert current is not None
                calls.append(current); current = None
            elif current is not None and event in current:
                current[event].append(dict(field.split('=', 1) for field in data.split('|')))
        assert current is None and len(calls) == 2 and 0 < sequence < 2000000
        arms[arm] = calls
    comparisons = []
    for i, (a, b) in enumerate(zip(arms['candidate'], arms['kissat'])):
        differences = {}
        for kind, keys in (('sweep_seed', ('external',)),
                           ('sweep_environment', ('external', 'vars', 'clauses')),
                           ('sweep_effort', ('external', 'delta', 'solves'))):
            differences[kind] = next((dict(index=j, candidate=x, kissat=y)
                for j, (x, y) in enumerate(itertools.zip_longest(a[kind], b[kind]))
                if x is None or y is None or [x.get(k) for k in keys] != [y.get(k) for k in keys]), None)
        comparisons.append(dict(call=i + 1, first_differences=differences,
            counts={arm: {k: len(v) for k, v in data.items()} for arm, data in (('candidate', a), ('kissat', b))},
            units=dict(candidate=a['sweep_unit'], kissat=b['sweep_unit']),
            equivalences=dict(candidate=a['sweep_equivalence'], kissat=b['sweep_equivalence'])))
    result = dict(parent_full_replay=True, parser_fixtures=14, evidence=evidence,
        event_counts=counts, outside_native=outside, calls=comparisons,
        limitation='Common environment sizes do not prove clause/model identity. This localizes missing work but neither identifies its cause nor establishes timing benefit. All parent traces and partial proof bytes replay exactly.')
    p = c / 'summary.json'
    if p.exists(): assert json.loads(p.read_text()) == json.loads(json.dumps(result))
    else: p.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'evidence'}, indent=2))

if __name__ == '__main__': main()
