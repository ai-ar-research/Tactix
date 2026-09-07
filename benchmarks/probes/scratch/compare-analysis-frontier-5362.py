#!/usr/bin/env python3
"""Retain the resolution/propagation witness without conflating physical refs."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3];c=root/'benchmarks/runs/20260907-5362-analysis-frontier';old=root/'benchmarks/runs/20260907-5361-queue-bumps'
m=json.loads((c/'manifest.json').read_text())
for rel,sha in m['hashes'].items():assert hashlib.sha256((c/rel).read_bytes()).hexdigest()==sha
events={}
for task,arm in enumerate(('tactix','kissat')):
    log=(c/'logs'/f'1668814_{task}.log').read_text();assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    for suffix in ('decisions','prefix','state','queue','bumps'):assert (c/'traces'/(arm+'.'+suffix+'.tsv')).read_bytes()==(old/'traces'/(arm+'.'+suffix+'.tsv')).read_bytes()
    raw=(c/'traces'/(arm+'.events.tsv')).read_text();assert 'trace_truncated' not in raw
    events[arm]=[r for l in raw.splitlines() if not l.startswith('#') and len(r:=l.split('\t'))>=22 and r[3]=='133186']
def payload(arm,event):return [r[-1] for r in events[arm] if r[2]==event]
def normalized_clause(p):return {k:v for part in p.split('|') if '=' in part for k,v in [part.split('=',1)] if k not in ('ref','id')}
assert normalized_clause(payload('tactix','conflict')[0])==normalized_clause(payload('kissat','conflict')[0])
assert payload('tactix','learned')==payload('kissat','learned')
assert normalized_clause(payload('tactix','learned_raw')[0])['set']==normalized_clause(payload('kissat','learned_raw')[0])['set']
seq={}
for arm,rows in events.items():
    bs=[]
    for r in rows:
        if r[2]=='analysis_resolve':bs.append(dict(pivot=r[-1],lits=[]))
        elif bs and r[2]=='analysis_reason_lit':bs[-1]['lits'].append(int(r[-1]))
    seq[arm]=bs
a,b=seq.values();i=next(i for i,(x,y) in enumerate(zip(a,b)) if x!=y)
assert i==28 and a[30]['pivot']==b[30]['pivot']=='20398'
result=dict(stopped_clean=2,previous_streams_byte_identical=True,conflict_equal_except_physical_ref_id=True,final_learned_equal=True,raw_learned_set_equal=True,first_resolution_block_difference=i,contexts={arm:rows[i-1:i+4] for arm,rows in seq.items()},implication_20398={arm:[p for p in payload(arm,'imply') if p.startswith('lit=20398|')] for arm in events},trace_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (c/'traces').iterdir()},conclusion='The same conflict and final learned clause conceal different resolution paths. At pivot 20398, Tactix uses reason -20396,-18559 while native uses -20347,-18559, after different implication order. Trace propagation upstream; do not adjust bump selection.')
dest=c/'summary.json'
if dest.exists():assert json.loads(dest.read_text())==result
else:dest.write_text(json.dumps(result,indent=2)+'\n')
print(result['conclusion'])
