"""Read Tactix's slot timers without treating them as native operation prices."""
import re


def read_timers(statistics):
    values = {}; rules = {}
    for line in statistics:
        if not line.startswith('c timing'):
            continue
        match = re.fullmatch(r'c timing\s+(\d+) ms solve', line)
        if match:
            if 'solve_ms' in values: raise ValueError('duplicate solve timer')
            values['solve_ms'] = int(match[1]); continue
        match = re.fullmatch(r'c timing\s+(\d+) ms \([\d.]+%\) inprocessing slot, (\d+) ms \([\d.]+%\) search\+scaffolding outside it', line)
        if match:
            if 'slot_ms' in values: raise ValueError('duplicate slot timer')
            values.update(slot_ms=int(match[1]), outside_ms=int(match[2])); continue
        match = re.fullmatch(r'c timing\s+([A-Za-z]+)\s+(\d+) ms \(\s*[\d.]+%\) in (\d+) applications', line)
        if match:
            if match[1] in rules: raise ValueError('duplicate rule timer')
            rules[match[1]] = dict(ms=int(match[2]), applications=int(match[3])); continue
        match = re.fullmatch(r'c timing\s+\(unattributed\)\s+(\d+) ms \(\s*[\d.]+%\)', line)
        if match:
            if 'unattributed_ms' in values: raise ValueError('duplicate unattributed timer')
            values['unattributed_ms'] = int(match[1]); continue
        raise ValueError('unrecognized timing row: '+line)
    if not {'solve_ms','slot_ms','outside_ms'} <= values.keys():
        raise ValueError('missing solve or slot timer')
    # main.cpp prints each quantity independently with %.0f (nearest integer).
    if abs(values['solve_ms']-values['slot_ms']-values['outside_ms']) > 1:
        raise ValueError('inconsistent slot partition')
    values['rules'] = rules
    return values
