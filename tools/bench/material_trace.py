"""Strict reader for a bounded native added-material producer trace."""
import re

FIELDS = ('conflicts', 'decisions', 'count', 'saved', 'external', 'source',
          'before', 'after', 'fixed', 'active', 'probing')


def parse_material(text, first, last, conflict_to):
    if any(not isinstance(x, int) or isinstance(x, bool) or x < 0 or x >= 1 << 64
           for x in (first, last, conflict_to)) or first > last:
        raise ValueError('invalid observation bounds')
    if not text or not text.endswith('\n'):
        raise ValueError('empty or incomplete material trace')
    lines = text.splitlines()
    if len(lines) != last - first + 1:
        raise ValueError('missing or duplicate material events')
    rows = []
    for ordinal, line in enumerate(lines):
        cells = line.split('\t')
        if len(cells) != len(FIELDS) + 1 or cells[0] != 'kissat':
            raise ValueError('invalid record shape')
        row = {}
        for key, cell in zip(FIELDS, cells[1:]):
            prefix = key + '='
            if not cell.startswith(prefix):
                raise ValueError('wrong or unordered material field')
            value = cell[len(prefix):]
            if key == 'source':
                if not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', value):
                    raise ValueError('invalid producer tag')
                row[key] = value
            elif key == 'external':
                if not re.fullmatch(r'-?[1-9][0-9]*', value) or abs(int(value)) > (1 << 31) - 1:
                    raise ValueError('invalid signed external literal')
                row[key] = int(value)
            else:
                if not value.isascii() or not value.isdecimal() or int(value) >= 1 << 64:
                    raise ValueError('invalid unsigned material operand')
                row[key] = int(value)
        if row['count'] != first + ordinal or row['conflicts'] > conflict_to:
            raise ValueError('record outside declared observation')
        if row['saved'] > row['count'] or row['before'] != 0 or row['after'] != 1:
            raise ValueError('invalid material transition')
        if any(row[k] not in (0, 1) for k in ('fixed', 'active', 'probing')):
            raise ValueError('invalid material flag')
        if rows and any(row[k] < rows[-1][k] for k in ('conflicts', 'decisions', 'saved')):
            raise ValueError('nonmonotone material history')
        rows.append(row)
    return rows
