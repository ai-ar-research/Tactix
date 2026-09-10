"""Strict selected-variable assignment and focused-queue diagnostic rows."""
import math
import re

FIELDS = ('conflicts decisions level chosen observed value assignment_level heap_position score reason '
          'stable queue_stamp search_external search_stamp observed_stamp observed_prev observed_next chosen_stamp').split()


def read_choice_state(content, *, conflict, variable):
    if not content or not content.endswith('\n') or conflict < 0 or variable <= 0:
        raise ValueError('missing or invalid selected choice state')
    rows = []
    for line in content.splitlines():
        values = line.split('\t')
        if len(values) != len(FIELDS):
            raise ValueError('invalid choice-state width')
        row = dict(zip(FIELDS, values))
        for key in FIELDS:
            if key in ('score', 'reason'):
                continue
            if not re.fullmatch(r'-?\d+' if key in ('chosen', 'value') else r'\d+', row[key]):
                raise ValueError('invalid choice-state integer')
            row[key] = int(row[key])
        row['score'] = float(row['score'])
        if not math.isfinite(row['score']) or row['score'] < 0:
            raise ValueError('invalid choice score')
        if row['conflicts'] != conflict or row['observed'] != variable or not row['chosen']:
            raise ValueError('wrong choice selection')
        if row['stable'] not in (0, 1) or row['value'] not in (-1, 0, 1):
            raise ValueError('invalid choice mode/value')
        if row['value'] == 0:
            if row['assignment_level'] != (1 << 32)-1 or row['reason'] != 'unassigned':
                raise ValueError('inconsistent unassigned variable')
        else:
            reason = row['reason']
            if row['assignment_level'] > row['level']:
                raise ValueError('assignment above current level')
            if reason not in ('decision', 'unit'):
                match = re.fullmatch(r'(binary|large):(-?\d+(?:,-?\d+)*)', reason)
                if not match or any(int(lit) == 0 for lit in match[2].split(',')):
                    raise ValueError('invalid assignment reason')
                if match[1] == 'binary' and ',' in match[2]:
                    raise ValueError('binary reason requires one literal')
        if rows and row['decisions'] <= rows[-1]['decisions']:
            raise ValueError('unordered or duplicated choice row')
        rows.append(row)
    return rows
