#!/usr/bin/env python3
"""Whole replay before bounded conflict-analysis interpretation; no timing claim."""
from collections import Counter
import datetime as dt
import importlib.util
import itertools
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from native_work import primary_statistics
from runtime_summary import counters
from search_window import read_search,macro_replay

C=ROOT/'benchmarks/runs/20260911-7401-rank43-analysis-window'
OLD=ROOT/'benchmarks/runs/20260911-7397-current-rank43-decisions'
TARGET=C/'solver-summary-7402.json'


def portable_payload(payload):
    if '=' not in payload:return payload
    result={}
    for part in payload.split('|'):
        key,sep,value=part.partition('=')
        assert sep and key not in result, 'malformed or duplicate payload field'
        result[key]=value
    return {k:v for k,v in result.items() if k not in ('ref','id')}


def fixtures():
    a='ref=12|id=34|order=1,-2|set=-2,1|levels=1,2'
    b=a.replace('ref=12','ref=56').replace('id=34','id=78')
    assert portable_payload(a)==portable_payload(b)
    for old,new in [('order=1,-2','order=-2,1'),('set=-2,1','set=-2,3'),('levels=1,2','levels=1,3')]:
        assert portable_payload(a)!=portable_payload(a.replace(old,new))
    for bad in ('a=1|a=2','a=1|broken'):
        try:portable_payload(bad)
        except AssertionError:pass
        else:raise AssertionError('bad payload accepted')
    assert portable_payload('-123')=='-123'
    print('Portable payload: seven physical-ID/order/set/level/framing controls PASS')


def validate(seal=False):
    if seal:assert not TARGET.exists(), 'successful evidence frozen'
    paths=[]
    for task in range(2):
        p=C/f'results/{task:03d}.json'
        if not p.exists():p=C/f'provisional/{task:03d}.json'
        if not p.exists():print('Solver record pending:',task);return
        paths.append(p)
    hashes={}
    def bind(p,expected=None):
        d=sha(p);assert expected is None or d==expected,str(p)
        hashes[str(p.relative_to(ROOT))]=d
    def read(p):bind(p);return json.loads(p.read_text())
    m=read(C/'manifest.json')
    for rel,d in m['pin_sha256'].items():bind(C/rel,d)
    for group in ('dependencies','protected_campaign_hashes'):
        for rel,d in m[group].items():bind(ROOT/rel,d)
    oldseal=read(OLD/'solver-summary-7398.json')
    nativeauthority=read(ROOT/'benchmarks/runs/20260907-5414-imply-fast/manifest.json')
    ns=nativeauthority['sources']['kissat'];bind(ROOT/ns['source']/ns['file'],ns['sha256'])
    normalizer=Path(__file__).with_name('validate-square-phase-5486.py');bind(normalizer)
    spec=importlib.util.spec_from_file_location('native7402',normalizer)
    normal=importlib.util.module_from_spec(spec);spec.loader.exec_module(normal);normal.self_test()
    allrows={};records={};replay={};window=tuple(m['window'])
    for task,arm in enumerate(('candidate','kissat')):
        p=paths[task];r=read(p);state=classify(r,p.parent.name=='results')
        assert state['status'] in ('pending_validation','checked_UNSAT'),state
        assert (r['task'],r['rank'],r['arm'])==(task,43,arm)
        assert r['solve']['solver_environment']==m['arms'][arm]['environment']
        oldpath=ROOT/oldseal['records'][arm]['path'];old=read(oldpath)
        for key in ('instance','expected','verdict','certificate_sha256','certificate_bytes'):
            assert r[key]==old[key],(arm,key)
        assert counters(r)==counters(old)
        if arm=='candidate':
            assert [s for s in r['solver_statistics'] if not s.startswith('c timing')]==[s for s in old['solver_statistics'] if not s.startswith('c timing')]
        else:assert primary_statistics(r['solver_statistics'])==primary_statistics(old['solver_statistics'])
        if p.parent.name=='results':assert r['binary_sha256']==m['arms'][arm]['sha256']
        info={}
        dp=C/'traces'/f'{arm}.decisions.tsv';op=OLD/'traces'/dp.name
        bind(dp,sha(op));info['whole_decision_stream_exact']=True
        bp=C/'traces'/f'{arm}.boundaries.tsv';op=OLD/'traces'/bp.name;bind(bp)
        if sha(bp)==sha(op):info['whole_boundaries_exact']=True
        else:
            assert arm=='kissat', 'candidate boundaries changed'
            def rows(path):
                assert path.read_bytes().endswith(b'\n')
                return [line.split('\t') for line in path.read_text().splitlines() if not line.startswith('#')]
            info['extra_analysis_end']=normal.validate_native_boundaries(rows(op),rows(bp),window)
        if arm=='candidate':
            mp=C/'traces/candidate.macro.tsv';bind(mp)
            info['macro_replay']=macro_replay((OLD/'traces/candidate.macro.tsv').read_text(),mp.read_text())
        sp=C/'traces'/f'{arm}.search.tsv';bind(sp)
        rr=read_search(sp.read_text(),solver='tactix' if arm=='candidate' else 'kissat',window=window,cap=m['row_cap'])
        selected=[row for row in rr if window[0]<=row['conflicts']<=window[1]]
        for conflict in range(window[0],window[1]+1):
            for event in ('conflict','analysis_begin','analysis_end','learned'):
                assert sum(row['conflicts']==conflict and row['event']==event for row in selected)==1,(arm,conflict,event)
        allrows[arm]=selected;replay[arm]=info
        records[arm]=dict(path=str(p.relative_to(ROOT)),state=state,work=counters(r),total_rows=len(rr),selected_events=dict(Counter(row['event'] for row in selected)))
    comparison={}
    for event in ('conflict','analysis_begin','learned_raw','learned_sorted','learned_shrunk','learned','analysis_end'):
        streams={arm:[row for row in rr if row['event']==event] for arm,rr in allrows.items()}
        first=None
        for i,(a,b) in enumerate(itertools.zip_longest(streams['candidate'],streams['kissat'])):
            def project(r):return None if r is None else (r['conflicts'],r['decisions'],r['level'],portable_payload(r['payload']))
            if project(a)!=project(b):first=dict(index=i,candidate=a,kissat=b);break
        comparison[event]=dict(counts={a:len(rr) for a,rr in streams.items()},first_portable_difference=first)
    for p in (Path(__file__),C/'launch.json',ROOT/'tools/bench/search_window.py',ROOT/'tools/bench/test_search_window.py'):
        bind(p)
    out=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,records=records,replay=replay,
        observer_replay_validated=True,comparison=comparison,
        limitation='Physical ref/id removed only from portable payload comparisons; clause order, sets, levels and every other field retained. Raw traces stay unchanged. Earliest difference is a localization point, not proof of cause or strength. Complete own work/proof/decision replay required; independent proofs and cleanup still gate acceptance.')
    if seal:
        with TARGET.open('x') as stream:json.dump(out,stream,indent=2)
        print(TARGET)
    print(json.dumps(comparison,indent=2))


if __name__=='__main__':
    assert sys.argv[1:] in ([],['--self-test'],['--seal'])
    fixtures()
    if sys.argv[1:]!=['--self-test']:validate(sys.argv[1:]==['--seal'])
