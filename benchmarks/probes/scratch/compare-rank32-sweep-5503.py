#!/usr/bin/env python3
"""Allow source-backed native prefix events and a retained schedule without enumeration."""
import collections
from pathlib import Path
import sys


def classify(arm, event, conflict):
    if conflict == 19375:
        return 'selected'
    assert arm == 'kissat' and 0 <= conflict < 19375
    assert event in ('sweep_schedule', 'sweep_seed', 'sweep_environment', 'sweep_effort')
    return 'earlier_native'


def self_test():
    assert classify('candidate', 'sweep_seed', 19375) == 'selected'
    assert classify('kissat', 'sweep_unit', 19375) == 'selected'
    for event in ('sweep_schedule', 'sweep_seed', 'sweep_environment', 'sweep_effort'):
        assert classify('kissat', event, 0) == 'earlier_native'
    for args in (('candidate', 'sweep_seed', 0), ('kissat', 'sweep_unit', 0),
                 ('kissat', 'sweep_seed', 19376), ('kissat', 'sweep_seed', -1)):
        try:
            classify(*args)
        except AssertionError:
            pass
        else:
            raise AssertionError('Unowned out-of-window event accepted')
    print('PASS: six positive/four negative Sweep window fixtures')


self_test()
if '--self-test' in sys.argv:
    raise SystemExit(0)
source = Path(__file__).with_name('compare-rank32-sweep-5490.py').read_text()
start = source.index('    events = collections.Counter()')
end = source.index('\nobservation = dict(', start)
source = source[:start] + '''    events, earlier = collections.Counter(), collections.Counter()
    total = 0
    for row in rows(c / 'traces' / (arm + '.sweep.tsv')):
        assert 'truncat' not in row[2].lower()
        assert int(row[1]) == total
        total += 1
        if row[2] == 'trace_begin':
            continue
        category = classify(arm, row[2], int(row[3]))
        (events if category == 'selected' else earlier)[row[2]] += 1
    assert 0 < total < 2000000
    assert events['sweep_seed'] and events['sweep_environment']
    # Tactix enumerates sweep_schedule only when no retained tail exists.
    # Native sweep.c emits these four families without detail-window checks;
    # record them separately, never treat them as selected-call evidence.
    neutral[arm] = dict(replayed_rows=count, events=dict(events),
                        earlier_native_events=dict(earlier), total_rows=total)
''' + source[end:]
exec(compile(source, __file__, 'exec'))
