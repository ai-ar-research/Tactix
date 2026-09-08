#!/usr/bin/env python3
"""Compare the complete post4824 Factor heap, not just its top element."""
import json
from pathlib import Path
import runpy
import sys

support = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root = support['root']

def heap(content, arm):
    lines = content.splitlines()
    assert lines and lines[0] == '# solver\tposition\texternal\tscore'
    result, seen = [], set()
    for line in lines[1:]:
        f = line.split('\t')
        assert len(f) == 4 and f[0] == arm and int(f[1]) == len(result)
        lit,score = int(f[2]),int(f[3])
        assert lit and lit not in seen and score >= 0
        if result: assert score <= result[(len(result)-1)//2][1]
        seen.add(lit); result.append((lit,score))
    return result

def fixtures():
    header = '# solver\tposition\texternal\tscore\n'
    good = header+'tactix\t0\t4\t8\ntactix\t1\t-3\t8\ntactix\t2\t2\t7\n'
    assert heap(good,'tactix') == [(4,8),(-3,8),(2,7)]
    assert heap(header,'tactix') == []
    for bad in ('',good.replace('tactix\t1','tactix\t2'),good.replace('\t-3\t8','\t4\t8'),
                good.replace('\t2\t7','\t2\t9'),good.replace('\t2\t7','\t2\t-1'),
                good.replace('tactix','kissat')):
        try: heap(bad,'tactix')
        except (AssertionError,ValueError): pass
        else: raise AssertionError('Invalid heap accepted')
    print('PASS: two positive/six negative heap fixtures',flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root/'benchmarks/runs/20260908-5571-rank32-factor-heap'
    if any(not (c/'results'/(a+'.json')).exists() for a in ('candidate','kissat')):
        print('5571 pending'); return
    evidence = support['validate'](c)
    data = {a:heap((c/'traces'/(a+'.heap.tsv')).read_text(),'tactix' if a=='candidate' else a)
            for a in ('candidate','kissat')}
    a,b = data['candidate'],data['kissat']
    assert a and b
    ac,bc = dict(a),dict(b)
    result = dict(parent_replay=True, parser_fixtures=8, evidence=evidence,
        counts={a:len(v) for a,v in data.items()}, physical_order_equal=a==b,
        membership_equal=ac.keys()==bc.keys(), scores_equal=ac==bc,
        candidate_only=sorted(ac.keys()-bc.keys()),kissat_only=sorted(bc.keys()-ac.keys()),
        unequal_scores=[dict(lit=l,candidate=ac[l],kissat=bc[l]) for l in sorted(ac.keys()&bc.keys()) if ac[l]!=bc[l]],
        first_position_difference=next((dict(index=i,candidate=x,kissat=y)
            for i,(x,y) in enumerate(zip(a,b)) if x!=y),None),
        limitation='Snapshot is after accepted grid4824. A score or topology difference may originate earlier; it does not prove the heap implementation is defective. Preserve public policy freedom and budget semantics.')
    p=c/'heap-localization.json'
    if p.exists(): assert json.loads(p.read_text()) == json.loads(json.dumps(result))
    else: p.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='evidence'},indent=2))

if __name__ == '__main__': main()
