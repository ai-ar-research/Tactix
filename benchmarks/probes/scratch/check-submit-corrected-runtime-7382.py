#!/usr/bin/env python3
"""Read provisional outcomes without proof waits; submit each four-task wave once."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
def matrix_contract(rows):
    order=[(rank,arm) for wave in range(3) for rank in (25,31)
           for arm in (('folded','legacy') if wave==1 else ('legacy','folded'))]
    assert len(rows)==12
    for t,(r,(rank,arm)) in enumerate(zip(rows,order)):
        assert int(r['task'])==t and int(r['rank'])==rank and r['arm']==arm
        assert r['tier']=='D' and r['expected']==('SAT' if rank==25 else 'UNSAT')
        if t%2:assert r['instance']==rows[t-1]['instance']
        if t>=4:assert r['instance']==rows[t%4]['instance']
def fixtures():
    rows=[dict(task=t,rank=rank,arm=arm,tier='D',instance=str(rank),expected='SAT' if rank==25 else 'UNSAT')
        for t,(rank,arm) in enumerate((rank,arm) for wave in range(3) for rank in (25,31)
            for arm in (('folded','legacy') if wave==1 else ('legacy','folded')))]
    matrix_contract(rows);bad=[rows[:-1],rows[::-1]]
    for t,k,v in [(0,'rank',26),(0,'expected','UNSAT'),(1,'instance','bad'),(4,'arm','legacy'),(8,'task',7),(0,'tier','B')]:
        r=[dict(x) for x in rows];r[t][k]=v;bad.append(r)
    for r in bad:
        try:matrix_contract(r)
        except AssertionError:pass
        else:raise AssertionError('bad wave matrix accepted')
    print('Root runtime matrix: eight negative controls PASS')
def module(name,file):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file));h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h);h.fixtures();return h
def main():
    fixtures();fp=module('fp','occurrence-fingerprint-7256.py');storage=module('storage','root-storage-statistics-7350.py')
    if sys.argv[1:]==['--self-test']:return
    args=sys.argv[1:];assert not args or len(args)==2 and args[0]=='--submit-wave' and args[1] in ('0','1','2')
    c=root/'benchmarks/runs/20260911-7382-corrected-root-runtime';m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest,rel
    for rel,digest in m['dependencies'].items():assert sha(root/rel)==digest,rel
    with (c/'matrix.tsv').open() as stream:rows=list(csv.DictReader(stream,delimiter='\t'))
    matrix_contract(rows);records={};states={}
    for folder in ('provisional','results'):
        for path in sorted((c/folder).glob('*.json')):
            r=json.loads(path.read_text());t=r['task'];assert t==int(path.stem) and 0<=t<12
            assert r['rank']==int(rows[t]['rank'])
            for key in ('arm','instance','expected'):assert r[key]==rows[t][key]
            state=classify(r,folder=='results');assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(path,state)
            if folder=='results':assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            if t in records:assert fp.raw_fingerprint(r)==fp.raw_fingerprint(records[t]) and r['solve']==records[t]['solve']
            records[t]=r;states[t]=state
    own={};pairs=[]
    for t,r in records.items():
        key=(r['rank'],r['arm'])
        if key in own:assert fp.raw_fingerprint(r)==fp.raw_fingerprint(own[key]),('own repeat drift',key,t)
        else:own[key]=r
    for t in range(0,12,2):
        if t not in records or t+1 not in records:continue
        a,b=records[t],records[t+1]
        if a['arm']=='folded':a,b=b,a
        pairs.append(dict(wave=t//4,rank=a['rank'],comparison=storage.compare(a,b)))
    exact=all(p['comparison']['storage_only'] for p in pairs)
    print(json.dumps(dict(records=len(records),checked=sum(s['checked'] for s in states.values()),
        cross_arm_storage_only=exact,pairs=pairs,observations=[dict(task=t,rank=r['rank'],arm=r['arm'],solve=r['solve']) for t,r in records.items()]),indent=2))
    if not args:return
    wave=int(args[1]);assert exact and all(t in records for t in range(4*wave))
    assert all(t not in records for t in range(4*wave,4*wave+4))
    launch=c/f'launch-wave{wave}.json';assert not launch.exists()
    for prior in range(wave):assert (c/f'launch-wave{prior}.json').exists()
    since=dt.datetime.now().replace(microsecond=0).isoformat()
    command=['sbatch','--parsable',f'--array={4*wave}-{4*wave+3}',str(c/'array.sbatch')]
    job=subprocess.check_output(command,text=True).strip().split(';')[0];assert job.isdigit()
    with launch.open('x') as out:json.dump(dict(job_id=job,accounting_since=since,command=command,manifest_sha256=sha(c/'manifest.json'),helper_sha256=sha(Path(__file__))),out,indent=2)
    print(job,since)
if __name__=='__main__':main()
