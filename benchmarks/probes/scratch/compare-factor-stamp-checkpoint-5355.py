#!/usr/bin/env python3
"""Reproduce 5352--5355 endpoint and producer comparisons, optionally seal once."""
import hashlib
import json
from pathlib import Path
import struct
import sys

root=Path(__file__).resolve().parents[3]
runs=root/'benchmarks/runs'
names={5352:'queue-stamps',5353:'factor-stamps',5354:'empty-factor',5355:'empty-factor-prefix'}
jobs={5352:1668720,5353:1668722,5354:1668724,5355:1668726}
def campaign(n):return runs/f'20260907-{n}-{names[n]}'
def blocks(path):
    result=[]
    for line in path.read_text().splitlines():
        r=line.split('\t')
        if r[0]=='snapshot':result.append(dict(header=r[1:],members=[]))
        else:
            assert r[0]=='member';result[-1]['members'].append(tuple(map(int,r[1:])))
    return result
def metadata(b):
    h=b['header']
    return dict(stage=h[0],conflicts=int(h[1]),decisions=int(h[2]),stable=int(h[3]),
        queue_stamp=int(h[4]),search_external=int(h[5]),search_stamp=int(h[6]),
        fresh_count=int(h[7]) if len(h)>7 and h[0]=='factor-stamps-pre' else None,
        queue_size=len(b['members']),members_sha256=hashlib.sha256(repr(b['members']).encode()).hexdigest())
def seal(n,result):
    c=campaign(n)
    result['trace_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (c/'traces').iterdir()}
    destination=c/'summary.json'
    if '--seal' in sys.argv:
        if destination.exists():assert json.loads(destination.read_text())==json.loads(json.dumps(result))
        else:destination.write_text(json.dumps(result,indent=2)+'\n')
    print(n,json.dumps({k:v for k,v in result.items() if k not in ('trace_sha256','snapshots','contexts','headers')}))
for n in names:
    for task,arm in enumerate(('tactix','kissat')):
        log=(campaign(n)/'logs'/f'{jobs[n]}_{task}.log').read_text()
        assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
q={n:{arm:blocks(campaign(n)/'traces'/(arm+'.queue.tsv')) for arm in ('tactix','kissat')} for n in (5352,5353,5354)}
for n in (5352,5353):
    for arm in ('tactix','kissat'):
        assert (campaign(n)/'traces'/(arm+'.schedule.tsv')).read_bytes()==(runs/'20260907-5351-bve-schedule'/'traces'/(arm+'.schedule.tsv')).read_bytes()
seal(5352,dict(stopped_clean=2,schedule_byte_identical_5351=True,
    snapshots={arm:[metadata(b) for b in bs] for arm,bs in q[5352].items()},
    conclusion='Both collect at startup, 1000 and 53839. Tactix retains one extra queue member 22152 after startup Factor; initial numeric stamp differences are overwritten by the first productive Factor. Inspect no-yield Factor restamping next.'))
for arm in ('tactix','kissat'):
    projected=[dict(header=b['header'][:7],members=b['members']) for b in q[5353][arm] if not b['header'][0].startswith('factor-stamps')]
    assert projected==q[5352][arm]
factor={arm:[b for b in bs if b['header'][0].startswith('factor-stamps')] for arm,bs in q[5353].items()}
assert len(factor['tactix'])==len(factor['kissat'])==30
empty=[]
for i in range(0,30,2):
    t0,t1=factor['tactix'][i:i+2];k0,k1=factor['kissat'][i:i+2]
    assert t0['header'][1:4]==k0['header'][1:4] and t0['header'][-1]==k0['header'][-1]
    if t0['header'][-1]!='0':continue
    assert t0['members']==t1['members'] and t0['header'][4:7]==t1['header'][4:7]
    assert [stamp for _,stamp in k1['members']]==list(range(1,len(k1['members'])+1))
    empty.append(dict(tactix_pre=metadata(t0),tactix_post=metadata(t1),kissat_pre=metadata(k0),kissat_post=metadata(k1)))
seal(5353,dict(stopped_clean=2,schedule_byte_identical_5351=True,previous_queue_projection_unchanged=True,
    factor_calls=15,empty_calls=len(empty),first_empty=empty[0],contexts=empty,
    conclusion='Executed zero-yield Factor passes omit stamp renumbering and cursor reset in Tactix; native performs both, beginning at conflict 7705.'))
rows={}
for arm in ('tactix','kissat'):
    lines=(campaign(5354)/'traces'/(arm+'.schedule.tsv')).read_text().splitlines()
    rows[arm]=[dict(x.split('=') for x in l.split()[1:]) for l in lines if l.startswith('member')]
a,b=rows.values();assert len(a)==len(b)==13003
keys=('external','position','positive','negative')
assert all(all(x[k]==y[k] for k in keys) for x,y in zip(a,b))
decode=lambda r:struct.unpack('>d',bytes.fromhex(r['score']))[0]
assert {decode(x)-decode(y) for x,y in zip(a,b)}=={-1.0}
seal(5354,dict(stopped_clean=2,matching_bve_members_positions_occurrences=13003,all_score_delta=-1.0,
    fixed_replay=json.loads((campaign(5354)/'fixture.json').read_text())['records'][0]['expected_exit'],
    conclusion='Empty Factor repair restores the entire initial heap pose; a uniform -1 score offset remains from the extra queue identity. No production promotion.'))
streams={};mask=(1<<64)-1
for arm in ('tactix','kissat'):
    c=campaign(5355)
    rows=[tuple(map(int,l.split())) for l in (c/'traces'/(arm+'.decisions.tsv')).read_text().splitlines()]
    assert all(len(r)==5 and r[0]<=200000 for r in rows)
    prefix=[l.split('\t') for l in (c/'traces'/(arm+'.prefix.tsv')).read_text().splitlines() if not l.startswith('#')]
    assert len(prefix)==1 and prefix[0][1]=='200000' and int(prefix[0][4])==len(rows)
    digest=1469598103934665603
    for r in rows:digest=((digest^(r[3]&mask))*1099511628211)&mask
    assert f'{digest:016x}'==prefix[0][5]
    streams[arm]=rows
assert (campaign(5355)/'traces/kissat.decisions.tsv').read_bytes()==(runs/'20260907-5347-decision-stream/traces/kissat.decisions.tsv').read_bytes()
a,b=streams.values();i=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
assert i==328665 and a[i]==(133243,327447,37,21863,0) and b[i]==(133243,327447,37,21954,0)
bves={}
for arm in ('tactix','kissat'):
    rows=[dict(x.split('=',1) for x in l.split()[1:]) for l in (campaign(5355)/'traces'/(arm+'.bve.tsv')).read_text().splitlines() if not l.startswith('#')]
    bves[arm]=[{k:r[k] for k in ('external','stage','conflicts','ticks','limit','applied','value')} for r in rows if r['stage'] in ('begin','end')]
x,y=bves.values();j=next((j for j,(u,v) in enumerate(zip(x,y)) if u!=v),min(len(x),len(y)))
seal(5355,dict(stopped_clean=2,full_fnv_verified=True,native_stream_unchanged=True,
    decision_counts=[len(a),len(b)],matching_decisions=i,first_tactix=a[i],first_kissat=b[i],
    contexts=dict(tactix=a[i-3:i+4],kissat=b[i-3:i+4]),
    bve_symmetric_counts=[len(x),len(y)],bve_matching_records=j,bve_equal=x==y,
    bve_first_tactix=x[j] if j<len(x) else None,bve_first_kissat=y[j] if j<len(y) else None,
    conclusion='The 95801 split closes; next first ordinary decision divergence is conflict 133243, level 37. Retain general empty Factor housekeeping and trace the next common-state owner.'))
