"""Require observed TransitiveReduce activity, not only requested options."""
import re

from inprocessing_cost import read_timers


def validate_activation(statistics, solver, enabled):
    if type(enabled) is not bool:
        raise ValueError('expected explicit Boolean mode')
    if solver == 'candidate':
        lines = [s for s in statistics if s.startswith('c transitive ')]
        if len(lines) != 1:
            raise ValueError('missing/duplicate current activity row')
        match = re.fullmatch(r'c transitive\s+(\d+) rounds, (\d+) checked, (\d+) deleted, (\d+) units, (\d+) ticks, (\d+) polls, (\d+) terminated', lines[0])
        if not match:
            raise ValueError('malformed current activity row')
        activity = dict(zip(('rounds', 'checked', 'deleted', 'units', 'ticks', 'polls', 'terminated'),
                            map(int, match.groups())))
        timers = read_timers(statistics)
        if enabled:
            if not activity['checked'] or not activity['ticks']:
                raise ValueError('enabled control did not execute transitive work')
        elif any(activity.values()) or 'TransitiveReduce' in timers['rules']:
            raise ValueError('disabled arm executed transitive work')
        return activity
    if solver == 'kissat':
        values = re.findall(r'^c\s+transitive_ticks:\s+(\d+)\b', '\n'.join(statistics), re.M)
        if len(values) != 1:
            raise ValueError('missing/duplicate native activity row')
        ticks = int(values[0])
        if bool(ticks) != enabled:
            raise ValueError('native activity disagrees with declared mode')
        return dict(ticks=ticks)
    raise ValueError('unsupported solver')
