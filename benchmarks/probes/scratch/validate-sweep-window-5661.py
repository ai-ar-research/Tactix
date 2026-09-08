#!/usr/bin/env python3
"""Normalize the native selected-window analysis/end observer, retaining all state fields."""
from pathlib import Path
import runpy
import sys
args = sys.argv[:]
sys.argv = [__file__, '--self-test']
namespace = runpy.run_path(str(Path(__file__).with_name('validate-sweep-observer-5660.py')))
sys.argv = args
original = namespace['prefix']

def prefix_with_analysis(current, previous):
    if current.name != 'kissat.boundaries.tsv': return original(current, previous)
    count, sequence, analysis = 0, 0, 0
    with current.open('rb') as src, previous.open('rb') as ref:
        for _ in range(2): assert next(src) == next(ref)
        for line in src:
            assert line.endswith(b'\n')
            row = line.rstrip(b'\n').split(b'\t')
            assert len(row) == 13 and int(row[1]) == sequence
            sequence += 1
            if row[2:5] == [b'analysis', b'end', b'120330']:
                analysis += 1
                continue
            row[1] = str(count).encode()
            assert row == next(ref).rstrip(b'\n').split(b'\t'), (current, count)
            count += 1
    assert count and analysis > 0
    return dict(matching_parent_rows=count, selected_analysis_end_rows=analysis,
        normalization='Only analysis/end at exact selected conflict120330 is separate;ordinary rows match all fields with sequence offset.')

namespace['validate_partial'].__globals__['prefix'] = prefix_with_analysis
namespace['main']()
