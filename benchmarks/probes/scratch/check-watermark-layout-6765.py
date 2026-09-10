#!/usr/bin/env python3
"""Require restored measured offsets and containment in formerly unused padding."""
from pathlib import Path
source = Path(__file__).with_name('inspect-watermark-layout-6760.py')
code = source.read_text()
changes = {
    '20260909-6760-watermark-layout': '20260909-6765-watermark-layout-control',
    "('candidate', '6730-reduce-watermark')": "('candidate', '6763-watermark-layout')",
    "out = dict(records=records, hashes=hashes,":
        "old = records['previous']['offsets']\nnew = records['candidate']['offsets']\nassert all(new[key] == value for key, value in old.items()), (old, new)\nassert old['large_watches_connected'] + 1 <= new['first_reducible']\nassert new['first_reducible'] + 4 <= old['arena_ordering']\nassert new['first_reducible'] % 4 == 0\nprint('PASS: every measured6654 member offset/size restored; marker fits original padding')\nout = dict(restored_offsets=True, marker_contained_in_original_padding=True, records=records, hashes=hashes,",
    'Compiled sizeof/offsetof evidence only, not runtime attribution. No solver or submitted source changed. C++ offsetof extension on this compiler; native solver not needed to measure a Tactix-only layout change.':
        'Compiler-level layout regression for isolated6763: all measured6654 offsets and size restored; marker fits former padding. Historical watermark semantics still require288-state/full-suite and exact certificate/work validation. This is not runtime attribution.',
    'Read-only fixed representation probe; compile and exact current/parent offsets are its oracle. No production test change needed. Any chosen layout ablation must preserve all288 watermark regressions and full work/proof replay.':
        'New fixed layout oracle for a member-only move; all measured old offsets and size plus marker containment asserted before summary. No new semantic cases; existing288-state and complete source suites cover behavior.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
