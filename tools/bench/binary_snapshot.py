"""Strict external sparse binary snapshots, including boundary identity."""
from congruence_trace import _uint


def read_snapshots(stream, solver, conflict, boundaries):
    assert solver in ('tactix','kissat') and boundaries
    assert stream.readline() == '# '+solver+'-binary-set-snapshot\t1\n', 'wrong schema'
    assert stream.readline() == '# snapshot\ta\tb\n', 'wrong columns'
    result,current = [],None
    for line in stream:
        assert line.endswith('\n'), 'truncated row'
        fields=line[:-1].split('\t')
        if fields[0]=='# begin':
            assert current is None and len(fields)==7, 'invalid begin'
            assert _uint(fields[1])==len(result) and fields[2]==solver, 'wrong identity'
            assert _uint(fields[4])==conflict, 'wrong conflict'
            assert (fields[3],fields[6])==boundaries[len(result)%len(boundaries)], 'wrong boundary'
            current=dict(event=fields[3],payload=fields[6],count=_uint(fields[5]),rows=[])
        elif fields[0]=='# end':
            assert current is not None and len(fields)==2, 'unframed end'
            assert _uint(fields[1])==len(result), 'wrong end ordinal'
            assert len(current['rows'])==current['count'], 'incomplete frame'
            assert current['rows']==sorted(current['rows']), 'unsorted snapshot'
            result.append(current); current=None
        else:
            assert current is not None and len(fields)==3, 'unframed pair'
            assert _uint(fields[0])==len(result), 'wrong pair ordinal'
            pair=tuple(map(int,fields[1:])); assert all(pair) and pair[0]<=pair[1], 'bad pair'
            assert len(current['rows'])<current['count'], 'excess pair'
            current['rows'].append(pair)
    assert current is None and result and len(result)%len(boundaries)==0, 'incomplete invocation'
    return result


def represented_values(formula):
    """Values only for variables present in the supplied large-clause snapshot."""
    result={}
    for (_,key),_,values in formula:
        assert len(key)==len(values)
        for lit,value in zip(key,values):
            variable=abs(lit); positive=value if lit>0 else -value
            assert lit and value in (-1,0,1)
            assert variable not in result or result[variable]==positive, 'inconsistent root value'
            result[variable]=positive
    return result
