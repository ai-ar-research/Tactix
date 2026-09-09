"""Strict bounded search rows and explicitly accounted macro observer additions."""
from collections import Counter

FIELDS = ('solver seq event conflicts decisions propagations level mode active unassigned trail '
          'queue heap retired_queue retired_heap irred red binary digest search_external top32 payload').split()
MACRO = ('solver seq event conflicts decisions propagations level mode unassigned trail restarts '
         'reductions switched rephased stamp').split()


def read_search(content, *, solver, window, cap):
    lines = content.splitlines()
    if not content.endswith('\n') or lines[:3] != ['# tactix-event-trace\t1',
            '# detail-conflicts\t' + '\t'.join(map(str, window)), '# ' + '\t'.join(FIELDS)]:
        raise ValueError('invalid search header/window')
    result = []
    for line in lines[3:]:
        values = line.split('\t')
        if len(values) != len(FIELDS):
            raise ValueError('invalid search row width')
        row = dict(zip(FIELDS, values))
        if row['solver'] != solver or not row['event'] or row['mode'] not in ('none', 'focused', 'stable'):
            raise ValueError('invalid search row identity')
        for key in ('seq', 'conflicts', 'decisions', 'propagations', 'level', 'active', 'unassigned',
                    'trail', 'queue', 'heap', 'retired_queue', 'retired_heap', 'irred', 'red', 'binary'):
            if not row[key].isdecimal():
                raise ValueError('invalid search counter')
            row[key] = int(row[key])
        row['search_external'] = int(row['search_external'])
        if row['seq'] != len(result) or (result and row['conflicts'] < result[-1]['conflicts']):
            raise ValueError('invalid search sequence')
        if len(row['digest']) != 16 or not 0 <= int(row['digest'], 16) < 1 << 64 or not row['payload']:
            raise ValueError('invalid digest/payload')
        result.append(row)
    if not result or len(result) >= cap:
        raise ValueError('empty or capped search trace')
    return result


def macro_replay(original, observed):
    def rows(content):
        lines = content.splitlines()
        if not content.endswith('\n') or lines[:2] != ['# tactix-macro-boundary\t1', '# ' + '\t'.join(MACRO)]:
            raise ValueError('invalid macro header')
        result = []
        for i, line in enumerate(lines[2:]):
            row = line.split('\t')
            if len(row) != len(MACRO) or row[0] != 'tactix' or row[1] != str(i):
                raise ValueError('invalid macro row/sequence')
            result.append(row[:1] + row[2:])
        return result
    before, after = rows(original), rows(observed)
    if not before:
        raise ValueError('empty original macro stream')
    index = 0; extras = []
    for row in after:
        if index < len(before) and row == before[index]:
            index += 1
        elif row[1] in ('trace_begin', 'retier', 'checkpoint'):
            extras.append(dict(zip(MACRO[:1] + MACRO[2:], row)))
        else:
            raise ValueError('original macro changed or unrecognized observer addition')
    if index != len(before):
        raise ValueError('missing original macro row')
    starts = [r for r in extras if r['event'] == 'trace_begin']
    if len(starts) > 1 or any(r['conflicts'] != '0' for r in starts):
        raise ValueError('invalid trace initialization')
    return dict(original_rows=len(before), extra_counts=dict(Counter(r['event'] for r in extras)), extras=extras)
