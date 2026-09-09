"""Selected-pair lifecycle metadata; payload remains an observer-specific string."""
from congruence_trace import _uint


def read_lifecycle(stream,solver,pairs):
    assert solver in ('tactix','kissat') and pairs
    fields='solver seq event conflicts search_ticks probing_ticks a b value_a value_b id copies payload'.split()
    if solver=='tactix': fields.insert(3,'rule')
    assert stream.readline()=='# tactix-binary-lifecycle\t3\n', 'wrong schema'
    assert stream.readline()=='# '+'\t'.join(fields)+'\n', 'wrong fields'
    result=[]
    for line in stream:
        assert line.endswith('\n'), 'truncated row'
        values=line[:-1].split('\t'); assert len(values)==len(fields), 'wrong width'
        r=dict(zip(fields,values)); assert r['solver']==solver, 'wrong solver'
        for key in ('seq','conflicts','search_ticks','probing_ticks','id','copies'): r[key]=_uint(r[key])
        assert r['seq']==len(result), 'bad sequence'
        assert not result or r['conflicts']>=result[-1]['conflicts'], 'conflict regression'
        for key in ('a','b','value_a','value_b'): r[key]=int(r[key])
        assert r['a'] and r['b'] and r['a']<=r['b'] and (r['a'],r['b']) in pairs, 'unselected pair'
        assert r['value_a'] in (-1,0,1) and r['value_b'] in (-1,0,1), 'invalid value'
        assert r['event'] and r['payload'] and r.get('rule','-'), 'missing metadata'
        result.append(r)
    return result
