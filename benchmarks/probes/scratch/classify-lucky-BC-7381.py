#!/usr/bin/env python3
"""Retain the strict BC failure, classify only four observed deactivation deltas."""
import copy
import datetime as dt
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
pattern=re.compile(r'c deactivate  (\d+) deactivations, (\d+) repeated reports, (\d+) re-bumps of retired vars')
def compare(a,b,expected):
    for key in ('instance','verdict','certificate_sha256','certificate_bytes'):
        assert a[key]==b[key],key
    def split(r):
        lines=[s for s in r['solver_statistics'] if not s.startswith('c timing')]
        hits=[i for i,s in enumerate(lines) if s.startswith('c deactivate ')]
        assert len(hits)==1
        i=hits[0];m=pattern.fullmatch(lines[i]);assert m
        values=tuple(map(int,m.groups()));lines[i]=('deactivate',values[1:])
        return lines,values
    x,dx=split(a);y,dy=split(b)
    assert x==y and dy[0]-dx[0]==expected,(dx,dy,expected)
    return dict(deactivation_delta=expected,current=dx,candidate=dy,
                all_other_non_timing_fields_and_certificate_exact=True)
def fixtures():
    a=dict(instance='x',verdict='UNSAT',certificate_sha256='a',certificate_bytes=1,
        solver_statistics=['c conflicts   3','c deactivate  10 deactivations, 2 repeated reports, 0 re-bumps of retired vars'])
    b=copy.deepcopy(a);b['solver_statistics'][1]=b['solver_statistics'][1].replace('10 deactivations','15 deactivations')
    compare(a,b,5);compare(a,a,0)
    bad=[]
    for key,value in [('instance','y'),('verdict','SAT'),('certificate_sha256','b'),('certificate_bytes',2)]:
        r=copy.deepcopy(b);r[key]=value;bad.append(r)
    for before,after in [('conflicts   3','conflicts   4'),('15 deactivations','16 deactivations'),('2 repeated','3 repeated'),('0 re-bumps','1 re-bumps')]:
        r=copy.deepcopy(b);r['solver_statistics']=[s.replace(before,after) for s in r['solver_statistics']];bad.append(r)
    for lines in [b['solver_statistics'][:1],b['solver_statistics']+[b['solver_statistics'][1]],b['solver_statistics']+['c unknown 1']]:
        r=copy.deepcopy(b);r['solver_statistics']=lines;bad.append(r)
    for r in bad:
        try:compare(a,r,5)
        except AssertionError:pass
        else:raise AssertionError('changed work or invalid deactivation schema accepted')
    print('Lucky BC classification: eleven negative controls PASS')
def main():
    fixtures()
    if sys.argv[1:]==['--self-test']:return
    assert not sys.argv[1:]
    target=root/'benchmarks/reports/2026-09-11-lucky-BC-classification-7381.json';assert not target.exists()
    reader=Path(__file__).with_name('check-submit-lucky-BC-7375.py')
    raw=subprocess.check_output([sys.executable,str(reader)],text=True)
    strict=json.loads(raw[raw.index('{'):]);assert strict['records']==66 and strict['exact_pairs']==29
    assert strict['status']=='CLASSIFICATION_REQUIRED'
    c=root/'benchmarks/runs/20260911-7375-lucky-BC';records={};hashes={};states={}
    def bind(p):hashes[str(p.relative_to(root))]=sha(p)
    for p in (reader,Path(__file__),c/'manifest.json',c/'launch.json'):bind(p)
    for t in range(66):
        p=c/f'results/{t:03d}.json';final=p.exists()
        if not final:p=c/f'provisional/{t:03d}.json'
        r=json.loads(p.read_text());s=classify(r,final)
        assert s['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
        bind(p);records[t]=r;states[t]=s
    pairs=[]
    for t in range(0,66,2):
        a,b=records[t],records[t+1];d=compare(a,b,{8:5,26:29,30:1,60:5}.get(t,0))
        pairs.append(dict(task=t,rank=a['rank'],instance=a['instance'],comparison=d,
            current_solve=a['solve'],candidate_solve=b['solve']))
    with target.open('x') as out:json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        status='EXACT_WORK_AND_CERTIFICATES_EXCEPT_FOUR_EXPLICIT_DEACTIVATION_COUNTS',
        strict_gate=strict,hashes=hashes,states=states,pairs=pairs,
        interpretation='Actual Lucky consumer repair increases four deactivation counts by5/29/1/5; all other raw non-timing fields and certificates match. Original strict gate remains CLASSIFICATION_REQUIRED; no generic fingerprint exclusion introduced.',
        limitation='This is not a speedup, full50 acceptance, complete queue trace, or a proof-check waiver.'),out,indent=2)
    print(target)
if __name__=='__main__':main()
