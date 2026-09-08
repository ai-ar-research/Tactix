#!/usr/bin/env python3
"""Validate observer neutrality, then compare Factor schedules/seeds/grids."""
import hashlib
import itertools
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5457-rank37-factor-window'
old=root/'benchmarks/runs/20260907-5453-rank37-eliminate-limit-prefix'
if len(list((c/'results').glob('*.json')))!=2:
    print('5457 pending');raise SystemExit(0)
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()
def rows(p):
    with p.open() as f:
        for line in f:
            if line.strip() and not line.startswith('#'):yield line.rstrip('\n').split('\t')
m=json.loads((c/'manifest.json').read_text())
for name,digest in m['hashes'].items():assert sha(c/name)==digest
events={};neutral={}
for arm in ('candidate','kissat'):
    result=json.loads((c/'results'/f'{arm}.json').read_text())
    assert result['exit_code']==0 and result['scratch_deleted'] and result['partial_certificate']['bytes']
    for name,digest in result['trace_sha256'].items():assert sha(c/'traces'/name)==digest
    previous=json.loads((old/'results'/f'{arm}.json').read_text())
    old_stream=old/'traces'/f'{arm}.decisions.tsv'
    assert sha(old_stream)==previous['trace_sha256'][old_stream.name]
    before=rows(old_stream);count=0
    for row in rows(c/'traces'/f'{arm}.decisions.tsv'):
        assert int(row[0])<m['stop_conflict']
        assert next(before)==row,(arm,count)
        count+=1
    assert int(next(before)[0])>=m['stop_conflict']
    neutral[arm]=count
    calls=[];current=None;records=0
    for row in rows(c/'traces'/f'{arm}.events.tsv'):
        records+=1
        event=row[2]
        assert 'truncat' not in event.lower(),event
        if event=='trace_begin':continue
        assert row[3]=='32073' and event.startswith('factor_'),row[:5]
        payload=dict(field.split('=',1) for field in row[-1].split('|') if '=' in field)
        if event=='factor_schedule' and payload['ordinal']=='0':
            current=dict(schedule=[],seeds=[],grids=[],members=[],summary=[])
            calls.append(current)
        if current is None:continue # native begin precedes schedule
        if event=='factor_schedule':current['schedule'].append({k:payload[k] for k in ('ordinal','external','score')})
        if event=='factor_seed':current['seeds'].append({k:payload[k] for k in ('external','remainders')})
        if event=='factor_grid':current['grids'].append({k:payload[k] for k in ('factors','remainders','delta')})
        if event=='factor_grid_member':current['members'].append(dict(ordinal=payload['ordinal'],external=payload['external'] if arm=='candidate' else payload['lit']))
        if event=='factor_summary':current['summary'].append(payload)
    assert records<2000000 and len(calls)==2,(arm,records,len(calls))
    events[arm]=calls
comparisons=[]
for ordinal,(t,k) in enumerate(zip(events['candidate'],events['kissat'])):
    comp=dict(call=ordinal+1,counts={a:{name:len(value) for name,value in call.items()} for a,call in [('candidate',t),('kissat',k)]},
              native_summary=k['summary'])
    for kind in ('schedule','seeds','grids','members'):
        first=None
        for index,(a,b) in enumerate(itertools.zip_longest(t[kind],k[kind])):
            if a!=b:
                first=dict(index=index,candidate=a,kissat=b);break
        comp[kind]=dict(equal=first is None,first_difference=first)
    comparisons.append(comp)
result=dict(clean_stops=2,observer_neutral_prefix_rows=neutral,conflict=32073,calls=comparisons,
    limitation='Controlled-stop diagnostic only. Matching counts are not formula or physical-state identity. First differing schedule or seed is a localization target, not automatically a solver defect or a performance improvement.')
p=c/'summary.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
