"""Fail-closed validation of selected watch-only additions to a search trace."""
from collections import Counter


def watch_payload(payload, scan):
    fields = {}
    for part in payload.split('|'):
        key, sep, value = part.partition('=')
        if not sep or not key or not value or key in fields:
            raise ValueError('invalid or repeated watch field')
        fields[key] = value
    required = {'scan', 'block', 'clause'}
    optional = {'id', 'order', 'garbage', 'searched', 'pose', 'values'}
    if not required <= fields.keys() or fields.keys() - required - optional:
        raise ValueError('unknown or missing watch field')
    if int(fields['scan']) != scan or not scan or not int(fields['block']):
        raise ValueError('wrong selected scan or invalid blocker')
    clause = list(map(int, fields['clause'].split(',')))
    if len(clause) < 2 or 0 in clause or clause != sorted(clause) or -scan not in clause:
        raise ValueError('invalid portable clause identity')
    if len(clause) > 2:
        if not {'pose', 'values', 'searched'} <= fields.keys():
            raise ValueError('missing large-clause state')
        pose = list(map(int, fields['pose'].split(',')))
        values = list(map(int, fields['values'].split(',')))
        if Counter(pose) != Counter(clause) or len(values) != len(pose) or set(values) - {-1, 0, 1}:
            raise ValueError('invalid clause pose or assignment state')
        if -scan not in pose[:2]:
            raise ValueError('selected literal is not watched')
    for key in ('id', 'order', 'garbage', 'searched'):
        if key in fields and not fields[key].isdecimal():
            raise ValueError('invalid watch metadata')
    return fields


def watch_replay(original, observed, *, scan, window):
    """Preserve every original field except sequence; retain every added row."""
    if not original or any(r['event'] == 'watch' for r in original):
        raise ValueError('expected nonempty watch-free control')
    index = 0
    extras = []
    semantic = lambda r: {k: v for k, v in r.items() if k != 'seq'}
    for row in observed:
        if row['event'] == 'watch':
            if not window[0] <= row['conflicts'] <= window[1]:
                raise ValueError('watch outside declared window')
            watch_payload(row['payload'], scan)
            extras.append(row)
        elif index < len(original) and semantic(row) == semantic(original[index]):
            index += 1
        else:
            raise ValueError('changed original or unrecognized observer addition')
    if index != len(original) or not extras:
        raise ValueError('missing original rows or empty watch observer')
    return extras
