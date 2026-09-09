"""Strict readers for the existing standalone Congruence census schemas."""
import re

BINARY_FIELDS = 'solver kind stage conflicts ordinal a b internal_a internal_b ticks'.split()
CANDIDATE_FIELDS = 'solver kind conflicts ordinal pose_a pose_b pose_c key_a key_b key_c x y entry exit delta disposition'.split()


def _rows(stream, kind, fields, solver, conflict):
    assert stream.readline() == '# tactix-congruence-' + kind + '\t1\n', 'wrong schema'
    assert stream.readline() == '# ' + '\t'.join(fields) + '\n', 'wrong columns'
    for line in stream:
        assert line.endswith('\n'), 'truncated row'
        values = line[:-1].split('\t')
        assert len(values) == len(fields), 'wrong row width'
        row = dict(zip(fields, values))
        assert row['solver'] == solver and int(row['conflicts']) == conflict, 'wrong trace identity'
        yield row


def _uint(value):
    assert value.isdecimal() and 0 <= int(value) < 1 << 64, 'invalid unsigned value'
    return int(value)


def read_binaries(stream, solver, conflict):
    groups = []
    current = None
    stages = (('sparse-entry', 'entry', 'post-binary-extraction', 'exit')
              if solver == 'kissat' else ('entry', 'post-binary-extraction', 'exit'))
    for row in _rows(stream, 'binaries', BINARY_FIELDS, solver, conflict):
        ordinal, ticks = _uint(row['ordinal']), _uint(row['ticks'])
        if row['kind'] == 'boundary':
            if current is not None:
                assert len(current['rows']) == current['count'], 'incomplete binary frame'
            assert row['stage'] == stages[len(groups) % len(stages)], 'wrong stage order'
            assert all(row[k] == '-' for k in ('a', 'b', 'internal_a', 'internal_b'))
            current = dict(stage=row['stage'], count=ordinal, ticks=ticks, rows=[])
            groups.append(current)
        else:
            assert row['kind'] == 'pair' and current is not None, 'unframed pair'
            assert row['stage'] == current['stage'] and ticks == current['ticks'], 'changed frame identity'
            assert ordinal == len(current['rows']) < current['count'], 'bad binary ordinal'
            a, b = int(row['a']), int(row['b'])
            assert a and b and a <= b, 'invalid sorted external pair'
            current['rows'].append((a, b, _uint(row['internal_a']), _uint(row['internal_b'])))
    assert groups and len(groups) % len(stages) == 0, 'missing complete invocation'
    assert len(current['rows']) == current['count'], 'incomplete final binary frame'
    return groups


def read_candidates(stream, solver, conflict):
    groups, current = [], None
    for row in _rows(stream, 'candidates', CANDIDATE_FIELDS, solver, conflict):
        entry, end, delta = (_uint(row[k]) for k in ('entry', 'exit', 'delta'))
        if row['kind'] == 'begin':
            assert current is None and entry == end == delta == 0, 'bad begin frame'
            assert all(row[k] == '-' for k in CANDIDATE_FIELDS[3:12] + ['disposition'])
            current = dict(rows=[])
        elif row['kind'] == 'candidate':
            assert current is not None and _uint(row['ordinal']) == len(current['rows']), 'bad candidate ordinal'
            assert entry == 3 * len(current['rows']) and end == entry + 3 and delta == 3, 'bad candidate effort'
            pose = tuple(int(row[k]) for k in ('pose_a', 'pose_b', 'pose_c'))
            key = tuple(int(row[k]) for k in ('key_a', 'key_b', 'key_c'))
            assert all(pose) and tuple(sorted(pose)) == key, 'changed clause identity'
            assert (row['x'] == '-') == (row['y'] == '-'), 'partial extraction pair'
            pair = None if row['x'] == '-' else (int(row['x']), int(row['y']))
            assert pair is None or all(pair), 'zero extraction literal'
            assert row['disposition'] in ('no-antecedent', 'entry-duplicate', 'extract'), 'unknown disposition'
            current['rows'].append(dict(pose=pose, key=key, pair=pair, disposition=row['disposition']))
        else:
            assert row['kind'] == 'end' and current is not None, 'unframed end'
            assert _uint(row['ordinal']) == len(current['rows']), 'wrong candidate count'
            assert entry == 0 and end == delta == 3 * len(current['rows']), 'wrong total effort'
            assert all(row[k] == '-' for k in CANDIDATE_FIELDS[4:12])
            match = re.fullmatch(r'complete=([01]);extracted=(\d+);production_ticks=(\d+)', row['disposition'])
            assert match, 'invalid completion metadata'
            current.update(complete=bool(int(match[1])), extracted=_uint(match[2]), production_ticks=_uint(match[3]))
            groups.append(current)
            current = None
    assert current is None and groups, 'incomplete candidate invocation'
    return groups
