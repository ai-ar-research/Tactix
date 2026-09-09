"""Read-only scheduling snapshot validation; actual sort differences are evidence."""
from collections import Counter
import re
import struct

FIELDS = 'ordinal compact external active value key_compact key_external score_bits stamp priority'.split()


def read_schedule(stream):
    assert next(stream) == '# transitive-schedule\t1\n', 'schema'
    snapshots = []
    current = None
    for line in stream:
        assert line.endswith('\n'), 'truncated row'
        cells = line[:-1].split('\t')
        if cells[0] == '# snapshot':
            assert current is None and len(cells) == 7, 'snapshot frame'
            _, stage, conflict, decisions, stable, ticks, size = cells
            assert stage == ('before' if len(snapshots) % 2 == 0 else 'after'), 'stage order'
            values = [int(v) for v in (conflict, decisions, stable, ticks, size)]
            assert all(0 <= v < 1 << 64 for v in values), 'snapshot integer'
            assert values[2] in (0, 1), 'mode'
            current = dict(stage=stage, conflict=values[0], decisions=values[1], stable=values[2],
                           ticks=values[3], size=values[4], rows=[])
            assert next(stream) == '# ' + '\t'.join(FIELDS) + '\n', 'columns'
        elif cells == ['# end']:
            assert current is not None and len(current['rows']) == current['size'], 'snapshot row count'
            rows = current['rows']
            assert len({r['compact'] for r in rows}) == len(rows), 'duplicate compact source'
            if current['stage'] == 'before':
                assert [r['compact'] for r in rows] == sorted(r['compact'] for r in rows), 'input seed order'
            else:
                before = snapshots[-1]
                for key in ('conflict', 'decisions', 'stable', 'ticks', 'size'):
                    assert current[key] == before[key], 'snapshot context: ' + key
                signature = lambda rs: Counter(tuple(r[k] for k in FIELDS[1:]) for r in rs)
                assert signature(rows) == signature(before['rows']), 'sort changed members or keys'
            snapshots.append(current)
            current = None
        else:
            assert current is not None and len(cells) == len(FIELDS), 'row width/placement'
            row = dict(zip(FIELDS, cells))
            assert re.fullmatch('[0-9a-f]{16}', row['score_bits']), 'score encoding'
            for key in FIELDS:
                if key == 'score_bits':
                    continue
                row[key] = int(row[key])
                if key in ('external', 'key_external', 'value'):
                    assert -(1 << 31) < row[key] < 1 << 31, key
                else:
                    assert 0 <= row[key] < 1 << 64, key
            assert row['ordinal'] == len(current['rows']), 'ordinal'
            assert row['compact'] < 1 << 32 and row['key_compact'] == row['compact'] >> 1, 'second IDX'
            assert row['external'] != 0 and row['active'] in (0, 1) and row['priority'] in (0, 1)
            assert row['value'] in (-1, 0, 1), 'value'
            current['rows'].append(row)
    assert current is None and snapshots and len(snapshots) % 2 == 0, 'unfinished snapshots'
    return snapshots


def sorting_key(row, stable):
    # Ascending native comparator; consumption is from the reverse end.
    priority = row['priority']
    if stable:
        score = struct.unpack('>d', bytes.fromhex(row['score_bits']))[0]
        return priority, score, row['key_compact']
    return priority, row['stamp']


def sorted_under_recorded_keys(snapshot):
    keys = [sorting_key(r, snapshot['stable']) for r in snapshot['rows']]
    return all(a <= b for a, b in zip(keys, keys[1:]))
