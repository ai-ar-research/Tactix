"""Readers for the existing complete stable-heap and selected-choice observers."""
import math

CHOICE_FIELDS = ('conflict', 'decision', 'level', 'chosen', 'wanted', 'value',
                 'assignment_level', 'heap_position', 'score', 'reason', 'stable',
                 'queue_stamp', 'queue_search', 'search_stamp', 'wanted_stamp',
                 'prev', 'next', 'chosen_stamp')


def score(text):
    value = float(text)
    if not math.isfinite(value) or value < 0:
        raise ValueError('invalid branch score')
    return text  # Preserve the observer's exact decimal rendering.


def read_heap(lines):
    lines = iter(lines)
    if next(lines, '').rstrip('\n') not in ('# tactix-heap-set\t1', '# kissat-heap-set\t1'):
        raise ValueError('unsupported heap header')
    if next(lines, '').rstrip('\n') != '# external\tposition\tscore\tassigned\tactive':
        raise ValueError('invalid heap columns')
    result = {}
    for index, line in enumerate(lines):
        fields = line.rstrip('\n').split('\t')
        if len(fields) != 5:
            raise ValueError('invalid heap row width')
        var, position, assigned, active = (int(fields[i]) for i in (0, 1, 3, 4))
        if var <= 0 or var in result or position != index or assigned not in (0, 1) or active not in (0, 1):
            raise ValueError('invalid heap identity, position or flag')
        result[var] = dict(position=position, score=score(fields[2]), assigned=assigned, active=active)
    return result


def compare_heaps(left, right):
    shared = sorted(left.keys() & right.keys())
    result = dict(candidate_only=sorted(left.keys() - right.keys()),
                  previous_only=sorted(right.keys() - left.keys()))
    for key in ('score', 'assigned', 'active', 'position'):
        result[key] = [dict(variable=v, candidate=left[v][key], previous=right[v][key])
                       for v in shared if left[v][key] != right[v][key]]
    return result


def read_choices(lines, *, conflict, wanted):
    result = []
    for line in lines:
        fields = line.rstrip('\n').split('\t')
        if len(fields) != len(CHOICE_FIELDS):
            raise ValueError('invalid choice row width')
        row = {name: fields[i] if name in ('score', 'reason') else int(fields[i])
               for i, name in enumerate(CHOICE_FIELDS)}
        score(row['score'])
        if row['conflict'] != conflict or row['wanted'] != wanted or not row['chosen']:
            raise ValueError('choice outside selected identity/window')
        if row['stable'] not in (0, 1) or row['value'] not in (-1, 0, 1):
            raise ValueError('invalid choice flag')
        for name in ('decision', 'level', 'assignment_level', 'heap_position', 'queue_stamp',
                     'search_stamp', 'wanted_stamp', 'chosen_stamp'):
            if row[name] < 0:
                raise ValueError('negative unsigned choice field')
        if result and row['decision'] != result[-1]['decision'] + 1:
            raise ValueError('missing, duplicated or reordered choice row')
        reason = row['reason']
        if row['value'] == 0:
            if reason != 'unassigned' or row['assignment_level'] != (1 << 32) - 1:
                raise ValueError('inconsistent unassigned state')
        else:
            if row['assignment_level'] > row['level']:
                raise ValueError('assignment beyond current level')
            if reason == 'unit' and row['assignment_level'] != 0:
                raise ValueError('unit reason above root')
            if reason not in ('decision', 'unit'):
                kind, separator, payload = reason.partition(':')
                if not separator or kind not in ('binary', 'large'):
                    raise ValueError('unknown assigned reason')
                lits = payload.split(',')
                if (kind == 'binary' and len(lits) != 1) or not all(int(lit) != 0 for lit in lits):
                    raise ValueError('invalid reason literals')
        result.append(row)
    if not result:
        raise ValueError('missing choice capture')
    return result
