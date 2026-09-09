"""Read a bound decision trace's complete threshold tail, including no rows.

This validates the tail framing, not the full trace: callers must independently
bind the complete file to a validated prior prefix and its recorded hash.
"""
import mmap
from pathlib import Path


def threshold_overlap(path, threshold):
    assert type(threshold) is int and threshold >= 0, 'invalid threshold'
    with Path(path).open('rb') as stream:
        assert stream.seek(0, 2) > 0, 'prior stream is empty'
        stream.seek(0)
        with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
            assert data[-1:] == b'\n', 'truncated prior decision row'
            end = len(data)
            start = end
            while end:
                begin = data.rfind(b'\n', 0, end - 1) + 1
                fields = data[begin:end - 1].split(b'\t')
                assert len(fields) == 5, 'decision stream must have five columns'
                row = list(map(int, fields))
                assert 0 <= row[0] <= threshold, 'prior row outside threshold'
                if row[0] < threshold:
                    break
                start, end = begin, begin
            return data[start:]
