"""Exact primary Kissat statistics, excluding derived rates and resource timers."""
import re


def primary_statistics(lines):
    """Require the complete statistics section; preserve every integer counter."""
    if not isinstance(lines, list) or not all(isinstance(x, str) for x in lines):
        raise ValueError('statistics must be text lines')
    marker = re.compile(r'^c -+ \[ ([^]]+) \] -+$')
    starts = [i for i, line in enumerate(lines)
              if (match := marker.fullmatch(line)) and match[1] == 'statistics']
    if len(starts) != 1:
        raise ValueError('one complete statistics section required')
    counts = {}; closed = False
    for line in lines[starts[0] + 1:]:
        if marker.fullmatch(line):
            closed = True; break
        match = re.fullmatch(r'c ([a-z][a-z0-9_]*):\s+([0-9]+)(?:\s+.*)?', line)
        if not match or match[1] in counts:
            raise ValueError('malformed or duplicate primary statistic')
        counts[match[1]] = int(match[2])
    if not closed or not counts:
        raise ValueError('truncated or empty statistics section')
    return counts
