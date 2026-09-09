#!/usr/bin/env python3
"""Require full replay before interpreting capacity-transition intervals."""
from pathlib import Path

source = Path(__file__).with_name('seal-collect-memory-6153.py')
code = source.read_text()
replacements = {
    "c = root / 'benchmarks/runs/20260908-6148-collect-memory'":
    "c = root / 'benchmarks/runs/20260909-6156-bve-capacity'",
    '20260908-6129-publication-memory/logs/': '20260908-6148-collect-memory/logs/',
    '20260908-6129-publication-memory/summary.json': '20260908-6148-collect-memory/summary.json',
    "slots = [row for row in rows if row['name'] != 'Collect']":
    "slots = [row for row in rows if row['name'] != 'BVECapacity']",
    'collector intervals sealed': 'capacity-transition intervals sealed',
    'Observed memory attribution,not Release timing or hard-memory acceptance. Collector temporary capacity is the other clause arena only,not all scratch. A peak first seen at entry precedes collector allocation. Intervals do notidentify unsampled suboperations.':
    'Capacity-transition interval attribution,not Release timing orhard-memory acceptance. Old6148 slot andcollector capacity sequence replays. A candidate may grow multiple pools; transient buffers can grow/free without a sampled capacity changing. Identify intervals,not individual allocations without finer evidence.',
}
for old, new in replacements.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
marker = '\ndef collections(rows):'
assert code.count(marker) == 1
code = code.replace(marker, "\nCAPACITY_KEYS += ('collector_order_capacity_bytes', 'collector_temporary_capacity_bytes')\n" + marker)
marker = '        groups = collections(rows)\n'
extra = '''        transitions = [row for row in rows if row['name'] == 'BVECapacity']
        assert transitions, 'missing capacity transitions'
        for row in transitions:
            assert row['stage'] in ('begin', 'end', 'postflush'), row
            assert 0 < row['memory_subject_internal'] <= row['vars'], row
            assert row['dense_large_arena_capacity_bytes'] + row['dense_ordered_arena_capacity_bytes'] <= row['dense_capacity_bytes'], row
'''
assert code.count(marker) == 1
code = code.replace(marker, marker + extra)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
