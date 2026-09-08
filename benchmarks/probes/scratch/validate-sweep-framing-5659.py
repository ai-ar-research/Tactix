#!/usr/bin/env python3
"""Account only for the event observer's explicit added trace_begin macro row."""
from pathlib import Path
source = Path(__file__).with_name('validate-partial-sweep-5658.py').read_text()
anchor = "counts[name] = prefix(c / 'traces' / name, parent / 'traces' / name)"
assert source.count(anchor) == 1
source = source.replace(anchor, "counts[name] = (macro_prefix if name.endswith('.macro.tsv') else prefix)(c / 'traces' / name, parent / 'traces' / name)")

def macro_prefix(current, previous):
    with current.open('rb') as src, previous.open('rb') as ref:
        for _ in range(2): assert next(src) == next(ref)
        added = next(src).rstrip(b'\n').split(b'\t')
        assert len(added) == 15 and added[:3] == [b'tactix', b'0', b'trace_begin']
        count = 0
        for line in src:
            assert line.endswith(b'\n')
            row = line.rstrip(b'\n').split(b'\t')
            assert len(row) == 15 and int(row[1]) == count + 1
            row[1] = str(count).encode()
            expected = next(ref).rstrip(b'\n').split(b'\t')
            assert row == expected, (current, count)
            if count == 0:
                assert row[2] == b'startup_begin' and added[3:] == row[3:]
            count += 1
        assert count > 0
        return dict(matching_parent_rows=count, added_trace_begin=1,
                    normalization='Only one initial trace_begin row and its sequence offset;every other field exact.')

exec(compile(source, __file__, 'exec'))
