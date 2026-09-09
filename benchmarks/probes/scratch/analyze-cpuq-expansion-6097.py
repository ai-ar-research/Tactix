#!/usr/bin/env python3
"""Select diagnostic additions from paired checked cpu-q solver results."""
from collections import Counter,defaultdict
import csv
import datetime as dt
import hashlib
import io
import json
from pathlib import Path
import re
import sys

root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s'
out=root/'benchmarks/reports/2026-09-08-cpuq-expansion'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def classify(r):
    if r is None:return 'pending'
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    if r['valid']:
        assert r['expected_match'] and r['solve']['exit_code'] in (10,20)
        return 'checked_'+r['verdict']
    if r['solve']['timed_out']:return 'solver_timeout'
    if (r.get('proof_check') or {}).get('timed_out'):return 'checker_timeout'
    return 'invalid_unscored'

def metric(t,k,timeout=1200):
    tc,kc=classify(t),classify(k)
    if kc not in ('checked_UNSAT','checked_SAT') or tc not in ('checked_UNSAT','checked_SAT','solver_timeout'):return None
    if tc!='solver_timeout':assert t['verdict']==k['verdict']
    kt=k['solve']['wall_seconds'];tt=timeout if tc=='solver_timeout' else t['solve']['wall_seconds']
    assert kt>0 and tt>=0
    return dict(verdict=k['verdict'],kissat_seconds=kt,tactix_seconds=tt,
                ratio=tt/kt,extra_seconds=tt-kt,ratio_is_lower_bound=tc=='solver_timeout',
                tactix_status=tc,kissat_status=kc)

def screen(row,max_native=120,ratio=1.5,gap=10):
    return row['kissat_seconds']<=max_native and row['ratio']>=ratio and row['extra_seconds']>=gap

def fixtures():
    def result(verdict,seconds,valid=True,to=False):
        return dict(artifact_deleted=True,derived_artifacts_deleted=True,cleanup_pending=False,
                    valid=valid,expected_match=valid,verdict=verdict,
                    solve=dict(exit_code=20 if verdict=='UNSAT' else 10,timed_out=to,wall_seconds=seconds))
    k=result('UNSAT',10);t=result('UNSAT',30)
    assert metric(t,k)['ratio']==3 and screen(metric(t,k))
    t=result('UNKNOWN',1200.01,False,True)
    assert metric(t,k)['ratio']==120 and metric(t,k)['ratio_is_lower_bound']
    t=result('UNSAT',300,False);t['proof_check']={'timed_out':True}
    assert classify(t)=='checker_timeout' and metric(t,k) is None
    assert metric(result('UNSAT',30),t) is None
    t['proof_check']={'timed_out':False};assert metric(t,k) is None
    assert metric(None,k) is None
    assert not screen(dict(kissat_seconds=2,ratio=4,extra_seconds=6))
    assert screen(dict(kissat_seconds=120,ratio=1.5,extra_seconds=60))
    assert not screen(dict(kissat_seconds=120.1,ratio=2,extra_seconds=120.1))
    print('PASS: checked pairs, timeout lower bounds, pending/checker-timeout/status exclusion, absolute gap and threshold endpoints')

def stats(r):
    rows=r.get('solver_statistics',[]);out={}
    for name in ('conflicts','decisions','propagations'):
        pattern=re.compile(r'^c\s+'+name+r'\s+([\d,]+)\b')
        for line in rows:
            m=pattern.search(line)
            if m:out[name]=int(m[1].replace(',',''));break
    out['timing_lines']=[s for s in rows if s.startswith('c timing')]
    return out

def main():
    fixtures()
    if '--self-test' in sys.argv:return
    assert not out.exists();out.mkdir()
    m=json.loads((c/'manifest.json').read_text())
    matrix=c/'matrix.tsv';matrix_rows=list(csv.DictReader(matrix.open(),delimiter='\t'))
    assert len(matrix_rows)==2332
    old_path=root/'benchmarks/runs/20260908-6061-root-lock-ranks1-40/matrix.tsv'
    old=list(csv.DictReader(old_path.open(),delimiter='\t'))
    assert len(old)==40 and [int(r['rank']) for r in old]==list(range(1,41))
    old_names={r['instance'] for r in old}
    hashes={str(p.relative_to(root)):sha(p) for p in (c/'manifest.json',matrix,old_path)}
    paired=defaultdict(dict);counts=Counter();provisional=Counter();pending=[]
    for row in matrix_rows:
        task=int(row['task']);p=c/'results'/f'{task:03d}.json'
        q=c/'provisional'/f'{task:03d}.json'
        assert q.exists()
        early=json.loads(q.read_text());assert early['instance']==row['instance'] and early['arm']==row['arm']
        hashes[str(q.relative_to(root))]=sha(q)
        provisional[(row['arm'],early['verdict'])]+=1
        r=json.loads(p.read_text()) if p.exists() else None
        if r:
            assert r['instance']==row['instance'] and r['arm']==row['arm'] and r['task']==task
            assert r['binary_sha256']==m['arms'][row['arm']]['sha256']
            hashes[str(p.relative_to(root))]=sha(p)
        state=classify(r);counts[(row['arm'],state)]+=1
        if state=='pending':pending.append(dict(task=task,instance=row['instance'],arm=row['arm'],solver_verdict=early['verdict']))
        paired[row['instance']][row['arm']]=r
    all_pairs=[];by_id={}
    for inst,pair in paired.items():
        assert set(pair)=={'tactix','kissat'}
        t,k=pair['tactix'],pair['kissat'];r=metric(t,k)
        if r is None:continue
        r.update(instance=inst,name=Path(inst).name.split('-',1)[1],cpuq_dataset_index=t['rank'],
                 existing_rank=next((int(o['rank']) for o in old if o['instance']==inst),None),
                 tactix_task=t['task'],kissat_task=k['task'],tactix_node=t['node'],kissat_node=k['node'],
                 tactix_work=stats(t),kissat_work=stats(k),
                 tactix_peak_rss_kb=t['solve'].get('peak_rss_kb'),kissat_peak_rss_kb=k['solve'].get('peak_rss_kb'))
        all_pairs.append(r);by_id[t['rank']]=r
    eligible=[r for r in all_pairs if r['instance'] not in old_names and screen(r,max_native=300)]
    # Explicit family-diverse proposal. This is benchmark selection, never solver policy.
    selected=[742,143,71,1146,1092,735,398,810,435,20]
    extension=[472,79,796,646,999,538,1110,456,749,207]
    reasons={742:'Fast UNSAT, formula-equivalence family; 2.37x.',
             143:'Fastest >=2x linked-list witness; shorter control for existing rank18.',
             71:'Stronger linked-list loss at a second unwind size; cap compact proposal at two additions.',
             1146:'UNSAT constraints family; independent of linked-list witnesses.',
             1092:'UNSAT oddball family; short moderate loss.',735:'UNSAT school-timetabling family.',
             398:'UNSAT MQTT verification family; moderate independent-family control.',
             810:'SAT: very fast native, >10x observed gap; complements existing cubes rank17.',
             435:'SAT: fast independent witness with >7x observed gap.',
             20:'SAT: >8x observed gap; complements existing MDP rank15.'}
    selected_rows=[]
    for rank,ident in enumerate(selected+extension,41):
        r=dict(by_id[ident]);assert r['instance'] not in old_names and screen(r,max_native=300)
        if rank<=50:assert screen(r,max_native=120)
        r.update(proposed_rank=rank,selection_reason=reasons.get(ident,'Optional broader/slower or related-family addition for Rank1-60.'))
        selected_rows.append(r)
    assert Counter(r['verdict'] for r in selected_rows[:10])=={'UNSAT':7,'SAT':3}
    for width in (50,60):
        rows=[dict(rank=int(r['rank']),instance=r['instance'],expected=r['expected']) for r in old]
        rows += [dict(rank=r['proposed_rank'],instance=r['instance'],expected=r['verdict']) for r in selected_rows[:width-40]]
        assert len(rows)==len({r['instance'] for r in rows})==width
        stream=io.StringIO();writer=csv.DictWriter(stream,fieldnames=('rank','instance','expected'),delimiter='\t',lineterminator='\n')
        writer.writeheader();writer.writerows(rows)
        (out/f'proposed-rank1-{width}.tsv').write_text(stream.getvalue())
    shortlist=sorted(eligible,key=lambda r:(r['verdict']!='UNSAT',r['kissat_seconds'],-r['ratio']))
    summary=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),campaign=str(c.relative_to(root)),
        source='cpu-q campaign uses older5741,not current6049; source/pin authority is the unchanged campaign manifest.',
        resources=m['resource_contract'],record_counts={f'{a}:{s}':n for (a,s),n in counts.items()},
        provisional_counts={f'{a}:{s}':n for (a,s),n in provisional.items()},pending=pending,
        screening=dict(compact='Kissat<=120s,ratio>=1.5,extra>=10s,checked native and checked same-verdict Tactix or solver timeout;exclude current40',
                       broad='Same but Kissat<=300s',strict='Kissat<=120s,ratio>=2,extra>=20s'),
        compact_unsat_count=sum(r['verdict']=='UNSAT' and screen(r) for r in eligible),
        strict_unsat_count=sum(r['verdict']=='UNSAT' and screen(r,ratio=2,gap=20) for r in eligible),
        shortlist=shortlist,proposed_additions=selected_rows,
        recommendation='Prefer Rank1-50 diagnostic expansion:7 UNSAT+3 SAT additions. Rank1-60 is optional; it adds slower cases and related families. User choice pending;no gate or runner changed.',
        limitations='One heterogeneous cpu-q paired window on older5741;not current6049 losses or isolated unit costs. CPU-q4CPU/32GiB timings cannot extend fixed12CPU/48000MiB denominator. Checker-invalid/pending results excluded from selection,not erased from campaign. Timeout ratios are lower bounds. Current40 and its goal remain intact;no new run submitted.',
        test_consideration='Selection fixtures distinguish checked solves, solver timeout lower bounds, checker failures/timeouts, pending outcomes, absolute-gap threshold and endpoint inclusion. Unique instance membership and old40 prefix asserted.',hashes=hashes)
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('hashes','shortlist','proposed_additions')},indent=2))
    for r in selected_rows:
        print(r['proposed_rank'],r['verdict'],round(r['kissat_seconds'],3),round(r['tactix_seconds'],3),round(r['ratio'],3),r['name'])

if __name__=='__main__':main()
