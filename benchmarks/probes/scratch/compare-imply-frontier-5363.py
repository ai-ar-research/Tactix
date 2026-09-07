#!/usr/bin/env python3
"""Retain the first post-inprocessing implication mismatch with neutral pins."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3];c=root/'benchmarks/runs/20260907-5363-imply-frontier';old=root/'benchmarks/runs/20260907-5362-analysis-frontier'
m=json.loads((c/'manifest.json').read_text())
for rel,sha in m['hashes'].items():assert hashlib.sha256((c/rel).read_bytes()).hexdigest()==sha
streams={}
for task,arm in enumerate(('tactix','kissat')):
    log=(c/'logs'/f'1668816_{task}.log').read_text();assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    for suffix in ('decisions','prefix','state','queue','bumps'):assert (c/'traces'/(arm+'.'+suffix+'.tsv')).read_bytes()==(old/'traces'/(arm+'.'+suffix+'.tsv')).read_bytes()
    raw=(c/'traces'/(arm+'.events.tsv')).read_text();assert 'trace_truncated' not in raw
    streams[arm]=[[r[3],r[4],r[-1]] for l in raw.splitlines() if not l.startswith('#') and len(r:=l.split('\t'))>=22 and r[2]=='imply' and 132817<=int(r[3])<=133186]
a,b=streams.values();i=next(i for i,(x,y) in enumerate(zip(a,b)) if x!=y)
assert i==1414 and a[i][:2]==b[i][:2]==['132832','326920']
result=dict(stopped_clean=2,previous_streams_byte_identical=True,matching_implications=i,counts={arm:len(rows) for arm,rows in streams.items()},contexts={arm:rows[i-3:i+4] for arm,rows in streams.items()},trace_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (c/'traces').iterdir()},conclusion='First post-phase implication difference at conflict 132832: native assigns 21214 immediately after common 21187, while Tactix assigns -21188 next. Inspect watch processing/pose; the later unequal resolution path is downstream.')
dest=c/'summary.json'
if dest.exists():assert json.loads(dest.read_text())==result
else:dest.write_text(json.dumps(result,indent=2)+'\n')
print(result['conclusion'])
