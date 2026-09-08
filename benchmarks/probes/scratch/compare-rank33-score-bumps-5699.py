#!/usr/bin/env python3
"""Compare every selected admission and post-bump value after complete replay."""
from collections import Counter
import itertools
import json
import math
from pathlib import Path
import runpy
import sys

header='# solver\tseq\tconflicts\tdecisions\texternal\tscore\tincrement\tstable\tposition\tlevel'
def rows(content,arm):
    assert content.endswith('\n')
    lines=content.splitlines()
    assert lines[:2]==['# '+arm+'-score-bumps\t1',header]
    result=[]
    for i,line in enumerate(lines[2:]):
        f=line.split('\t');assert len(f)==10 and f[0]==arm
        seq,conflict,decision,external=map(int,f[1:5]);score,increment=map(float,f[5:7])
        stable,position,level=map(int,f[7:])
        assert seq==i and 0<=conflict<=182093 and decision>=0
        assert external in (120425,120434,120438) and stable==1
        assert math.isfinite(score) and score>=0 and math.isfinite(increment) and increment>0
        assert 0<=position<=0xffffffff and 0<=level<=0xffffffff
        if result:assert conflict>=result[-1]['conflicts'] and decision>=result[-1]['decisions']
        result.append(dict(seq=seq,conflicts=conflict,decisions=decision,external=external,
            score=score,increment=increment,stable=stable,position=position,level=level))
    assert result
    return result
def fixtures():
    good='# tactix-score-bumps\t1\n'+header+'\ntactix\t0\t10\t20\t120438\t1.5\t1\t1\t4294967295\t0\n'
    assert len(rows(good,'tactix'))==1
    assert len(rows(good.replace('tactix','kissat'),'kissat'))==1
    bad=[good.rstrip('\n'),good.replace('bumps\t1','bumps\t2'),good.replace('\t0\t10','\t1\t10'),
        good.replace('\t10\t20','\t182094\t20'),good.replace('120438','120439'),good.replace('1.5','nan'),
        good.replace('1.5','-1'),good.replace('1.5\t1','1.5\t0'),good.replace('1.5\t1\t1','1.5\t1\t0'),
        good.replace('4294967295','4294967296'),good+'\n',
        good+good.splitlines()[-1].replace('\t0\t10\t20','\t1\t9\t21')+'\n',
        good+good.splitlines()[-1].replace('\t0\t10\t20','\t1\t11\t19')+'\n']
    for content in bad:
        try:rows(content,'tactix')
        except (AssertionError,ValueError):pass
        else:raise AssertionError('Malformed score bump accepted')
    print('PASS: two positive/thirteen negative score-bump fixtures',flush=True)
def main():
    fixtures()
    if '--self-test' in sys.argv:return
    support=runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
    c=support['root']/'benchmarks/runs/20260908-5698-rank33-score-bumps'
    if any(not (c/'results'/(a+'.json')).exists() for a in ('candidate','kissat')):
        print('5698 pending');return
    evidence=support['validate'](c)
    histories={a:rows((c/'traces'/(a+'.bumps.tsv')).read_text(),'tactix' if a=='candidate' else a)
        for a in ('candidate','kissat')}
    t,k=histories['candidate'],histories['kissat']
    admission=lambda r:(r['conflicts'],r['decisions'],r['external']) if r else None
    first=next((i for i,(x,y) in enumerate(itertools.zip_longest(t,k)) if admission(x)!=admission(y)),None)
    values=next((i for i,(x,y) in enumerate(zip(t,k)) if admission(x)==admission(y) and
        (x['score'],x['increment'])!=(y['score'],y['increment'])),None)
    tc,kc=(Counter(admission(r) for r in history) for history in (t,k))
    surplus=lambda counter:[dict(admission=key,count=count) for key,count in sorted(counter.items())]
    context=lambda i:{a:history[max(0,i-3):i+5] for a,history in histories.items()} if i is not None else None
    result=dict(parent_full_replay=True,parser_fixtures=15,evidence=evidence,
        row_counts={a:len(h) for a,h in histories.items()},
        first_admission_difference_index=first,admission_context=context(first),
        first_equal_admission_value_difference_index=values,value_context=context(values),
        candidate_only=surplus(tc-kc),kissat_only=surplus(kc-tc),
        limitation='Admissions compare only selected analyzed-variable bump callbacks. Native/Tactix differing admission order or reason graph must be localized before repair. Current increment can be rescaled inside a bump; no runtime claim.')
    p=c/'summary.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:value for key,value in result.items() if key!='evidence'},indent=2))
if __name__=='__main__':main()
