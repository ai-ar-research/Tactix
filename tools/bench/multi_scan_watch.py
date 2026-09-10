"""Validate a clause-variable-filtered watch trace against its prior scan."""
from watch_observer import watch_payload


def parse_watch(row):
    if row['event'] != 'watch':
        raise ValueError('not a strict watch row')
    first = row['payload'].split('|', 1)[0]
    if not first.startswith('scan='):
        raise ValueError('missing leading scan')
    scan = int(first[5:])
    return watch_payload(row['payload'], scan)


def multi_scan_projection(original, observed, *, window, variable, prior_scan):
    if type(variable) is not int or variable <= 0 or type(prior_scan) is not int or not prior_scan:
        raise ValueError('invalid selected variable or control scan')
    old = []
    for row in original:
        if not window[0] <= row['conflicts'] <= window[1]:
            continue
        fields = parse_watch(row)
        if int(fields['scan']) != prior_scan:
            raise ValueError('wrong control scan')
        if variable in map(abs, map(int, fields['clause'].split(','))):
            old.append(row)
    new = []
    for row in observed:
        if not window[0] <= row['conflicts'] <= window[1]:
            raise ValueError('watch outside declared window')
        fields = parse_watch(row)
        if variable not in map(abs, map(int, fields['clause'].split(','))):
            raise ValueError('watch outside selected clause-variable scope')
        if int(fields['scan']) == prior_scan:
            new.append(row)
    normalize = lambda rows: [{k:v for k,v in row.items() if k != 'seq'} for row in rows]
    if not old or normalize(old) != normalize(new):
        raise ValueError('missing or changed control projection')
    return len(new)
