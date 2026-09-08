#!/usr/bin/env python3
"""Account for source-backed active-observer macro rows without relaxing state replay."""
from pathlib import Path
source = Path(__file__).with_name('validate-sweep-framing-5659.py').read_text()
start, end = source.index('def macro_prefix('), source.index('\nexec(compile(')
source = source[:start] + '''def macro_prefix(current, previous):
    from collections import Counter
    extras, count, sequence = Counter(), 0, 0
    with current.open('rb') as src, previous.open('rb') as ref:
        for _ in range(2): assert next(src) == next(ref)
        terminated = False
        for line in src:
            assert not terminated and line.endswith(b'\\n')
            row = line.rstrip(b'\\n').split(b'\\t')
            assert len(row) == 15 and int(row[1]) == sequence
            sequence += 1
            event = row[2]
            if event in (b'trace_begin', b'retier', b'checkpoint', b'terminate'):
                if event == b'trace_begin': assert sequence == 1 and row[3:6] == [b'0', b'0', b'1']
                if event == b'terminate': terminated = True
                extras[event.decode()] += 1
                continue
            row[1] = str(count).encode()
            assert row == next(ref).rstrip(b'\\n').split(b'\\t'), (current, count)
            count += 1
        assert count and extras['trace_begin'] == 1 and extras['terminate'] == 1
    return dict(matching_parent_rows=count, added_observer_rows=extras,
        normalization='Only active-observer trace_begin/retier/checkpoint and final timeout terminate are separate;all ordinary macro fields and complete observed decision/boundary rows remain exact.')
''' + source[end:]
exec(compile(source, __file__, 'exec'))
