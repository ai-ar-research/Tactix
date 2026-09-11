#!/usr/bin/env python3
"""Join full candidate/native replays and compare complete analysis transactions."""
from collections import Counter
import datetime as dt
import importlib.util
import itertools
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from native_work import primary_statistics
from runtime_summary import counters
from search_window import read_search,macro_replay

C=ROOT/'benchmarks/runs/20260911-7403-rank43-native-analysis-complete'
PRIOR=ROOT/'benchmarks/runs/20260911-7401-rank43-analysis-window'
BASE=ROOT/'benchmarks/runs/20260911-7397-current-rank43-decisions'
TARGET=C/'solver-summary-7404.json'


def module(filename):
    p=Path(__file__).with_name(filename)
    spec=importlib.util.spec_from_file_location(filename,p)
    h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h);return h


def transactions(rows,core_end=82):
    pending=None;conflict=None;result=[]
    for row in rows:
        event=row['event']
        if event=='conflict':
            assert pending is None and conflict is None, 'unpaired conflict'
            conflict=row
        elif event=='analysis_begin':
            assert pending is None, 'missing analysis end'
            if conflict is not None:assert (conflict['conflicts'],conflict['decisions'])==(row['conflicts'],row['decisions'])
            elif row['conflicts']<=core_end:raise AssertionError('missing core conflict payload')
            pending=dict(begin=row,conflict=conflict,stages=[]);conflict=None
        elif event=='analysis_end':
            assert pending is not None, 'missing analysis begin'
            begin=pending['begin']
            assert row['conflicts']>=begin['conflicts'] and row['decisions']==begin['decisions']
            pending['end']=row;result.append(pending);pending=None
        elif pending is not None and event.startswith('learned'):
            pending['stages'].append(row)
    assert pending is None and conflict is None, 'incomplete final transaction'
    assert result and result[0]['begin']['conflicts']==1 and result[-1]['end']['conflicts']==85
    # Every conflict counter is accounted by a transaction, including an
    # internal OTFS advance; no mandatory learned emission on a reused conflict.
    next_conflict=1
    for tx in result:
        assert tx['begin']['conflicts']==next_conflict, 'unexplained conflict gap'
        next_conflict=tx['end']['conflicts']+1
    return result


def fixtures():
    def row(e,c,d=1):return dict(event=e,conflicts=c,decisions=d,payload='x',level=1)
    good=[row('conflict',1),row('analysis_begin',1),row('analysis_end',2),
          row('conflict',3),row('analysis_begin',3),row('learned',3),row('analysis_end',85)]
    assert len(transactions(good))==2
    missing_outside=good+[row('analysis_begin',86),row('analysis_end',85)]
    bad=[good[1:],good[:-1],good[:2]+good[3:],good[:3]+good[4:],
         good[:2]+[row('analysis_begin',2)]+good[2:],
         good[:2]+[row('analysis_end',2,2)]+good[3:],
         [row('analysis_end',1)]+good,missing_outside]
    for rr in bad:
        try:transactions(rr)
        except AssertionError:pass
        else:raise AssertionError('bad transaction accepted')
    outside=[row('conflict',1),row('analysis_begin',1),row('analysis_end',83),row('analysis_begin',84),row('analysis_end',85)]
    assert transactions(outside)[1]['conflict'] is None
    print('Transactions: OTFS/reuse/outside-core omission positive controls and8 negative cases PASS')


def validate(seal=False):
    if seal:assert not TARGET.exists()
    paths={}
    for arm,c in (('candidate',PRIOR),('kissat',C)):
        p=c/'results/000.json'
        if not p.exists():p=c/'provisional/000.json'
        if not p.exists():print(arm,'record pending');return
        paths[arm]=p
    hashes={}
    def bind(p,d=None):
        actual=sha(p);assert d is None or actual==d,str(p)
        hashes[str(p.relative_to(ROOT))]=actual
    def read(p):bind(p);return json.loads(p.read_text())
    s=read(BASE/'solver-summary-7398.json');failure=read(PRIOR/'native-truncation-7403.json')
    h=module('validate-rank43-analysis-7402.py');h.fixtures()
    bind(Path(__file__).with_name('validate-rank43-analysis-7402.py'),failure['strict_validator_sha256'])
    n=module('validate-square-phase-5486.py');n.self_test()
    for filename in ('validate-square-phase-5486.py','validate-rank43-transactions-7404.py'):
        bind(Path(__file__).with_name(filename))
    result={};txs={}
    for arm,c in (('candidate',PRIOR),('kissat',C)):
        m=read(c/'manifest.json');bind(c/'launch.json')
        for rel,d in m['pin_sha256'].items():bind(c/rel,d)
        for group in ('dependencies','protected_campaign_hashes'):
            for rel,d in m[group].items():bind(ROOT/rel,d)
        p=paths[arm];r=read(p);state=classify(r,p.parent.name=='results')
        assert state['status'] in ('pending_validation','checked_UNSAT'),state
        assert (r['task'],r['rank'],r['arm'])==(0,43,arm)
        assert r['solve']['solver_environment']==m['arms'][arm]['environment']
        old=read(ROOT/s['records'][arm]['path'])
        for key in ('instance','expected','verdict','certificate_sha256','certificate_bytes'):
            assert r[key]==old[key],(arm,key)
        assert counters(r)==counters(old)
        if arm=='candidate':assert [x for x in r['solver_statistics'] if not x.startswith('c timing')]==[x for x in old['solver_statistics'] if not x.startswith('c timing')]
        else:assert primary_statistics(r['solver_statistics'])==primary_statistics(old['solver_statistics'])
        if p.parent.name=='results':assert r['binary_sha256']==m['arms'][arm]['sha256']
        dp=c/'traces'/f'{arm}.decisions.tsv';bind(dp,sha(BASE/'traces'/dp.name))
        bp=c/'traces'/f'{arm}.boundaries.tsv';bind(bp)
        info=dict(whole_decisions_exact=True,whole_work_certificate_exact=True)
        if arm=='candidate':
            assert sha(bp)==sha(BASE/'traces'/bp.name)
            mp=c/'traces/candidate.macro.tsv';bind(mp)
            info['macro']=macro_replay((BASE/'traces/candidate.macro.tsv').read_text(),mp.read_text())
        else:
            def b_rows(p):
                assert p.read_bytes().endswith(b'\n')
                return [l.split('\t') for l in p.read_text().splitlines() if not l.startswith('#')]
            info['extra_analysis_end']=n.validate_native_boundaries(b_rows(BASE/'traces'/bp.name),b_rows(bp),(1,85))
        sp=c/'traces'/f'{arm}.search.tsv';bind(sp)
        rows=read_search(sp.read_text(),solver='tactix' if arm=='candidate' else 'kissat',window=(1,85),cap=m['row_cap'])
        if arm=='kissat':
            prior=PRIOR/'traces/kissat.search.tsv';bind(prior)
            earlier=[l for l in prior.read_text().splitlines()[3:] if not l.startswith('#')]
            assert len(earlier)==250000
            assert sp.read_text().splitlines()[3:3+len(earlier)]==earlier
            info['exact_capped_prefix_rows']=len(earlier)
        selected=[r for r in rows if 1<=r['conflicts']<=85]
        txs[arm]=transactions(selected)
        result[arm]=dict(record=str(p.relative_to(ROOT)),state=state,replay=info,rows=len(rows),transactions=len(txs[arm]),
            internal_counter_advances=[(t['begin']['conflicts'],t['end']['conflicts']) for t in txs[arm] if t['begin']['conflicts']!=t['end']['conflicts']],
            missing_conflict_payloads=[t['begin']['conflicts'] for t in txs[arm] if t['conflict'] is None])
    comparisons={}
    for field in ('conflict','begin','stages','end'):
        first=None
        for i,(a,b) in enumerate(itertools.zip_longest(txs['candidate'],txs['kissat'])):
            if a is not None and a['begin']['conflicts']>82:break
            def row(r):return None if r is None else dict(event=r['event'],conflicts=r['conflicts'],decisions=r['decisions'],level=r['level'],payload=h.portable_payload(r['payload']))
            def project(tx):
                if tx is None:return None
                return [row(r) for r in tx[field]] if field=='stages' else row(tx[field])
            if project(a)!=project(b):first=dict(index=i,candidate=project(a),kissat=project(b));break
        comparisons[field]=first
    out=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,records=result,
        observer_replay_validated=True,core_conflicts=[1,82],first_transaction_differences=comparisons,
        limitations='Original7401 native cap and invalid per-integer learned-event assumption remain preserved. Full uncapped traces, complete own work/proof/decisions, exact captured native prefix and balanced transactions now required. Internal OTFS advances and reused-conflict paths are explicit. Native missing C84 conflict payload is outside the C1..82 causal core and is not accepted as formula equality. Different hook emissions are not automatically semantic differences or defects; no timing/strength claim.')
    if seal:
        with TARGET.open('x') as stream:json.dump(out,stream,indent=2)
        print(TARGET)
    print(json.dumps(dict(records=result,first_transaction_differences=comparisons),indent=2))


if __name__=='__main__':
    assert sys.argv[1:] in ([],['--self-test'],['--seal'])
    fixtures()
    if sys.argv[1:]!=['--self-test']:validate(sys.argv[1:]==['--seal'])
