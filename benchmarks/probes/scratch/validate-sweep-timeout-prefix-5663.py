#!/usr/bin/env python3
"""Require every captured byte, including the SIGTERM-cut final row, to match."""
import io
from pathlib import Path
import runpy
import sys

def streams(src, ref):
    count, tail = 0, 0
    for line in src:
        expected = next(ref)
        if line.endswith(b'\n'):
            assert expected == line
            count += 1
        else:
            assert expected.startswith(line) and len(line) < len(expected)
            assert src.read() == b''
            tail = len(line)
    assert count > 0
    return dict(complete_parent_lines=count, final_parent_row_prefix_bytes=tail)

def fixtures():
    assert streams(io.BytesIO(b'a\nb\n'), io.BytesIO(b'a\nb\nc\n')) == dict(complete_parent_lines=2, final_parent_row_prefix_bytes=0)
    assert streams(io.BytesIO(b'a\nb'), io.BytesIO(b'a\nbee\nc\n')) == dict(complete_parent_lines=1, final_parent_row_prefix_bytes=1)
    for src, ref in [(b'', b'a\n'), (b'x\n', b'a\n'), (b'a\nx', b'a\nbee\n'), (b'a\nbeee', b'a\nbee\n')]:
        try: streams(io.BytesIO(src), io.BytesIO(ref))
        except (AssertionError, StopIteration): pass
        else: raise AssertionError('Unmatched timeout prefix accepted')
    print('PASS: two positive/four negative partial-row fixtures', flush=True)

def partial_prefix(current, previous):
    with current.open('rb') as src, previous.open('rb') as ref:
        return streams(src, ref)

fixtures()
args = sys.argv[:]
sys.argv = [__file__, '--self-test']
namespace = runpy.run_path(str(Path(__file__).with_name('validate-sweep-window-5661.py')))
sys.argv = args
namespace['prefix_with_analysis'].__globals__['original'] = partial_prefix
namespace['namespace']['main']()
