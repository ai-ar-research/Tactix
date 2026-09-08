#!/usr/bin/env python3
"""Validate all score rows and report common-boundary bracketing, not causation."""
from collections import Counter
import json
import math
from pathlib import Path
import runpy
import sys

header = '# solver\tlabel\tconflicts\tdecisions\texternal\tscore\tposition\tvalue\tactive'
def rows(content,arm):
    assert content.endswith('\n')
    lines = content.splitlines()
    assert lines[:2] == ['# '+arm+'-inprocess-score\t1',header]
    result = []
    for line in lines[2:]:
        f = line.split('\t')
        assert len(f) == 9 and f[0] == arm and f[1]
        conflict,decision,external = map(int,f[2:5])
        score = float(f[5]); position = None if f[6] == '-' else int(f[6])
        value,active = map(int,f[7:])
        assert conflict >= 0 and decision >= 0 and external == 120438
        assert math.isfinite(score) and score >= 0
        assert position is None or 0 <= position < 0xffffffff
        assert value in (-1,0,1) and active in (0,1)
        if result:
            assert conflict >= result[-1]['conflicts'] and decision >= result[-1]['decisions']
        result.append(dict(row=len(result)+3,label=f[1],conflicts=conflict,decisions=decision,
            external=external,score=score,position=position,value=value,active=active))
    assert result
    return result

def fixtures():
    good='# tactix-inprocess-score\t1\n'+header+'\ntactix\trestart_begin\t10\t20\t120438\t1.5\t-\t0\t1\n'
    assert rows(good,'tactix')[0]['position'] is None
    assert rows(good.replace('tactix','kissat').replace('\t-\t','\t0\t'),'kissat')[0]['position']==0
    bad=[good.rstrip('\n'),good.replace('score\t1','score\t2'),good.replace('restart_begin',''),
        good.replace('\t10\t20','\t-1\t20'),good.replace('120438','120439'),
        good.replace('1.5','nan'),good.replace('1.5','-1'),good.replace('\t-\t','\t-1\t'),
        good.replace('\t0\t1\n','\t2\t1\n'),good.replace('\t0\t1\n','\t0\t2\n'),
        good+'\n',good+good.splitlines()[-1].replace('\t10\t20','\t9\t21')+'\n',
        good+good.splitlines()[-1].replace('\t10\t20','\t11\t19')+'\n']
    for content in bad:
        try: rows(content,'tactix')
        except (AssertionError,ValueError): pass
        else: raise AssertionError('Malformed score history accepted')
    print('PASS: two positive/thirteen negative score-history fixtures',flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv:return
    support=runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
    c=support['root']/'benchmarks/runs/20260908-5694-rank33-score-history'
    if any(not (c/'results'/(a+'.json')).exists() for a in ('candidate','kissat')):
        print('5694 pending');return
    evidence=support['validate'](c)
    histories={a:rows((c/'traces'/(a+'.scores.tsv')).read_text(),'tactix' if a=='candidate' else a)
        for a in ('candidate','kissat')}
    maps={}
    for arm,history in histories.items():
        counts=Counter(); mapping={}
        for r in history:
            if r['conflicts']>182093:continue
            key=(r['conflicts'],r['decisions'],r['label'])
            ordinal=counts[key];counts[key]+=1
            mapping[(*key,ordinal)]=r
        maps[arm]=mapping
    t,k=maps['candidate'],maps['kissat']
    common=sorted(t.keys()&k.keys(),key=lambda key:t[key]['row'])
    assert common and [k[key]['row'] for key in common]==sorted(k[key]['row'] for key in common)
    first=next((i for i,key in enumerate(common) if t[key]['score']!=k[key]['score']),None)
    pair=lambda key:dict(key=key,candidate=t[key],kissat=k[key])
    result=dict(parent_full_replay=True,parser_fixtures=15,evidence=evidence,
        row_counts={a:len(h) for a,h in histories.items()},common_boundary_count=len(common),
        first_score_difference=pair(common[first]) if first is not None else None,
        preceding_common_boundary=pair(common[first-1]) if first else None,
        unequal_common_score_count=sum(t[key]['score']!=k[key]['score'] for key in common),
        unmatched_before_split={a:[mapping[key] for key in mapping if key not in maps['kissat' if a=='candidate' else 'candidate']]
            for a,mapping in maps.items()},
        limitation='All unmatched rows retained. Same event/counter/ordinal snapshots bracket a difference, not its exact causal update; active flags and within-conflict snapshot phases can be representation-local.')
    p=c/'summary.json';assert not p.exists()
    p.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:value for key,value in result.items() if key not in ('evidence','unmatched_before_split')},indent=2))

if __name__=='__main__':main()
