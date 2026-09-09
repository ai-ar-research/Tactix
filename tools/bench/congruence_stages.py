"""Additional observer stages and strict large-clause snapshot validation."""
import re
from congruence_trace import BINARY_FIELDS, _rows, _uint


def read_stages(stream, conflict):
    stages = ('entry', 'post-binary-extraction', 'installed', 'gates-built', 'exit')
    groups, current = [], None
    for row in _rows(stream, 'binaries', BINARY_FIELDS, 'tactix', conflict):
        ordinal, ticks = _uint(row['ordinal']), _uint(row['ticks'])
        if row['kind'] == 'boundary':
            assert current is None or len(current['rows']) == current['count'], 'incomplete frame'
            assert row['stage'] == stages[len(groups) % len(stages)], 'wrong stage order'
            assert all(row[k] == '-' for k in ('a','b','internal_a','internal_b'))
            current = dict(stage=row['stage'], count=ordinal, ticks=ticks, rows=[])
            groups.append(current)
        else:
            assert row['kind'] == 'pair' and current is not None, 'unframed pair'
            assert row['stage'] == current['stage'] and ticks == current['ticks'], 'changed frame identity'
            assert ordinal == len(current['rows']) < current['count'], 'bad ordinal'
            a,b = int(row['a']),int(row['b'])
            assert a and b and a <= b, 'invalid external pair'
            current['rows'].append((a,b,_uint(row['internal_a']),_uint(row['internal_b'])))
    assert groups and len(groups) % len(stages) == 0, 'incomplete invocation'
    assert len(current['rows']) == current['count'], 'incomplete final frame'
    return groups


def read_formula(stream, solver):
    """Yield exact identity, physical pose and sorted root values; hashes are metadata."""
    assert solver in ('tactix','kissat')
    assert stream.readline() == '# '+solver+'-formula-set\t2\n', 'wrong schema'
    assert stream.readline() == '# redundant\tsize\thash\tliterals\n', 'wrong columns'
    for line in stream:
        assert line.endswith('\n'), 'truncated row'
        row = line[:-1].split('\t')
        assert len(row) == 6, 'wrong width'
        red,size,digest,literals,pose,values = row
        assert red in ('0','1') and _uint(size) >= 3, 'invalid clause metadata'
        assert re.fullmatch('[0-9a-f]{16}',digest), 'invalid hash'
        key = tuple(map(int,literals.split(',')))
        assert len(key) == int(size) and all(key) and tuple(sorted(key)) == key, 'bad clause identity'
        assert pose.startswith('pose=') and values.startswith('values='), 'wrong optional fields'
        ordered = tuple(map(int,pose[5:].split(',')))
        assert tuple(sorted(ordered)) == key, 'pose changes identity'
        pairs = tuple(tuple(map(int,p.split(':'))) for p in values[7:].split(','))
        assert all(len(p)==2 for p in pairs), 'bad value pair'
        assert tuple(p[0] for p in pairs) == key, 'values change identity'
        assert all(p[1] in (-1,0,1) for p in pairs), 'bad root value'
        yield (int(red),key), ordered, tuple(p[1] for p in pairs)
