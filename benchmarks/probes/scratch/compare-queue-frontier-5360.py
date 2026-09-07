#!/usr/bin/env python3
"""Verify queue-boundary observer neutrality and shared-identity order."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5360-queue-frontier';old=root/'benchmarks/runs/20260907-5359-focused-choice'
m=json.loads((c/'manifest.json').read_text())
for rel,sha in m['hashes'].items():assert hashlib.sha256((c/rel).read_bytes()).hexdigest()==sha
blocks={}
for task,arm in enumerate(('tactix','kissat')):
    log=(c/'logs'/f'1668810_{task}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    for suffix in ('decisions','prefix','state'):
        assert (c/'traces'/(arm+'.'+suffix+'.tsv')).read_bytes()==(old/'traces'/(arm+'.'+suffix+'.tsv')).read_bytes()
    bs=[]
    for l in (c/'traces'/(arm+'.queue.tsv')).read_text().splitlines():
        r=l.split('\t')
        if r[0]=='snapshot':bs.append(dict(header=r[1:],members=[]))
        else:bs[-1]['members'].append(list(map(int,r[1:])))
    blocks[arm]=[b for b in bs if b['header'][0].startswith('factor-stamps')]
assert len(blocks['tactix'])==len(blocks['kissat'])==38
contexts=[]
for t,k in zip(blocks['tactix'],blocks['kissat']):
    assert t['header'][:4]==k['header'][:4]
    a=[v for v,s in t['members']];b=[v for v,s in k['members']];shared=set(a)&set(b)
    assert [v for v in a if v in shared]==[v for v in b if v in shared]
    contexts.append(dict(header=t['header'][:4],shared=len(shared),tactix_only=sorted(set(a)-set(b)),kissat_only=sorted(set(b)-set(a)),observed_neighbors=dict(tactix=a[a.index(21954)-1:a.index(21954)+2],kissat=b[b.index(21954)-1:b.index(21954)+2])))
assert contexts[-1]['header'][1]=='132816'
result=dict(stopped_clean=2,previous_streams_byte_identical=True,matching_factor_boundaries=38,contexts=contexts,trace_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (c/'traces').iterdir()},conclusion='All shared-identity queue order matches through the last Factor boundary at 132816. The focused choice split arises afterward; inspect active search queue bumps.')
dest=c/'summary.json'
if dest.exists():assert json.loads(dest.read_text())==result
else:dest.write_text(json.dumps(result,indent=2)+'\n')
print(result['conclusion'])
