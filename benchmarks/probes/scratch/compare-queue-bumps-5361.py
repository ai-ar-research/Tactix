#!/usr/bin/env python3
"""Verify bump observer neutrality and retain first changed membership."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3];c=root/'benchmarks/runs/20260907-5361-queue-bumps';old=root/'benchmarks/runs/20260907-5360-queue-frontier'
m=json.loads((c/'manifest.json').read_text())
for rel,sha in m['hashes'].items():assert hashlib.sha256((c/rel).read_bytes()).hexdigest()==sha
streams={}
for task,arm in enumerate(('tactix','kissat')):
    log=(c/'logs'/f'1668812_{task}.log').read_text();assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    for suffix in ('decisions','prefix','state','queue'):assert (c/'traces'/(arm+'.'+suffix+'.tsv')).read_bytes()==(old/'traces'/(arm+'.'+suffix+'.tsv')).read_bytes()
    streams[arm]=[list(map(int,l.split())) for l in (c/'traces'/(arm+'.bumps.tsv')).read_text().splitlines()]
a,b=streams.values()
i=next(i for i,(x,y) in enumerate(zip(a,b)) if x[:4]!=y[:4])
assert i==27585 and a[i][:4]==[133186,327383,0,20396] and b[i][:4]==[133186,327383,0,20508]
assert all(x[4]-y[4]==x[9]-y[9]==1 and x[5:7]==y[5:7] and x[10]==y[10] for x,y in zip(a[:i],b[:i]))
cursor_diffs=[j for j,(x,y) in enumerate(zip(a[:i],b[:i])) if x[7]!=y[7] or x[8]-y[8]!=1]
assert len(cursor_diffs)==53
aa=[r[3] for r in a if r[0]==133186];bb=[r[3] for r in b if r[0]==133186]
result=dict(stopped_clean=2,previous_streams_byte_identical=True,matching_active_bumps=i,matching_pre_links_values_and_uniform_stamp_offset=True,cursor_differences=dict(count=53,first_index=cursor_diffs[0],tactix=a[cursor_diffs[0]],kissat=b[cursor_diffs[0]],interpretation='Transient cursor mapping differs while queue links, moved identities and all decision bytes still agree. No claim of complete heuristic-state parity.'),counts={arm:len(rows) for arm,rows in streams.items()},contexts={arm:rows[i-2:i+3] for arm,rows in streams.items()},first_conflict_membership=dict(tactix_count=len(aa),kissat_count=len(bb),tactix_only=sorted(set(aa)-set(bb)),kissat_only=sorted(set(bb)-set(aa))),trace_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (c/'traces').iterdir()},conclusion='Queue links, moved identities and values agree through 27585 moves; stamps differ uniformly by one. Transient cursor differences are retained separately. Bump membership first differs at 133186; trace conflict/reason analysis upstream.')
dest=c/'summary.json'
if dest.exists():assert json.loads(dest.read_text())==result
else:dest.write_text(json.dumps(result,indent=2)+'\n')
print(result['conclusion'])
