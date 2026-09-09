"""Validate selected-variable macro score histories without losing repeated sites."""
from collections import Counter
import math

FIELDS = ('solver', 'label', 'conflicts', 'decisions', 'external', 'score', 'position', 'value', 'active')


def read(content, *, solver, external):
    if not content.endswith('\n'):
        raise ValueError('unterminated score history')
    lines = content.splitlines()
    if lines[:2] != [f'# {solver}-inprocess-score\t1', '# ' + '\t'.join(FIELDS)]:
        raise ValueError('invalid score history header')
    result = []
    for line_number, line in enumerate(lines[2:], 3):
        values = line.split('\t')
        if len(values) != len(FIELDS) or values[0] != solver or not values[1]:
            raise ValueError('invalid score row schema')
        row = dict(zip(FIELDS, values))
        for key in ('conflicts', 'decisions', 'external', 'value', 'active'):
            row[key] = int(row[key])
        row['score'] = float(row['score'])
        row['position'] = None if row['position'] == '-' else int(row['position'])
        row['row'] = line_number
        if row['conflicts'] < 0 or row['decisions'] < 0 or row['external'] != external:
            raise ValueError('invalid score identity or counter')
        if not math.isfinite(row['score']) or row['score'] < 0:
            raise ValueError('invalid score')
        if row['position'] is not None and not 0 <= row['position'] < (1 << 32) - 1:
            raise ValueError('invalid heap position')
        if row['value'] not in (-1, 0, 1) or row['active'] not in (0, 1):
            raise ValueError('invalid score state')
        if result:
            previous = result[-1]
            if row['conflicts'] < previous['conflicts']:
                raise ValueError('conflict counter regressed')
            # The solver resets warm-up decision counters at conflict zero.
            # Preserve those rows and report the reset; never use the derived
            # prefix start counter as a monotonicity premise.
            if row['decisions'] < previous['decisions'] and row['conflicts'] != 0:
                raise ValueError('search decision counter regressed')
        result.append(row)
    if not result:
        raise ValueError('missing score rows')
    return result


def compare(left, right, *, before):
    def index(rows):
        counts = Counter()
        mapped = {}
        for row in rows:
            if (row['conflicts'], row['decisions']) > before:
                continue
            key = (row['conflicts'], row['decisions'], row['label'])
            ordinal = counts[key]
            counts[key] += 1
            mapped[(*key, ordinal)] = row
        return mapped
    a, b = index(left), index(right)
    common = [key for key in a if key in b]
    if not common:
        raise ValueError('no aligned score sites')
    if [b[key]['row'] for key in common] != sorted(b[key]['row'] for key in common):
        raise ValueError('shared score sites changed order')
    pair = lambda key: dict(key=key, candidate=a[key], previous=b[key])
    differences = {}
    for field in ('score', 'position', 'value', 'active'):
        changed = [i for i, key in enumerate(common) if a[key][field] != b[key][field]]
        first = changed[0] if changed else None
        differences[field] = dict(count=len(changed), first=pair(common[first]) if first is not None else None,
            preceding=pair(common[first - 1]) if first is not None and first > 0 else None)
    return dict(common_sites=len(common), differences=differences,
        unmatched=dict(candidate=[row for key, row in a.items() if key not in b],
                       previous=[row for key, row in b.items() if key not in a]))
