#!/usr/bin/env python3
"""Retain first new decision split and its preceding formula-state owner."""
import hashlib
import itertools
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5373-million-prefix'
old=root/'benchmarks/runs/20260907-5370-congruence-prefix-extended'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''): h.update(b)
 return h.hexdigest()
m=json.loads((c/'manifest.json').read_text())
for rel,expected in m['hashes'].items(): assert sha(c/rel)==expected
for i,arm in enumerate(('tactix','kissat')):
 log=(c/'logs'/f'1668911_{i}.log').read_text()
 assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
 before=(old/'traces'/f'{arm}.decisions.tsv').read_bytes()
 with (c/'traces'/f'{arm}.decisions.tsv').open('rb') as f: assert f.read(len(before))==before
with (c/'traces/tactix.decisions.tsv').open() as a,(c/'traces/kissat.decisions.tsv').open() as b:
 first=next((i,x.rstrip().split('\t'),y.rstrip().split('\t')) for i,(x,y) in enumerate(itertools.zip_longest(a,b)) if x!=y)
assert first[0]==786346 and first[1][:3]==first[2][:3]==['345034','783732','257']
names={'congruence':'Congruence','substitute_early':'Substitute','substitute_late':'Substitute',
 'backbone_early':'Backbone','backbone_late':'Backbone','vivify':'Vivify','sweep':'Sweep',
 'transitive':'TransitiveReduce','factor':'Factor','strengthen':'Strengthen','bve':'BVE'}
boundaries={}
for arm in ('tactix','kissat'):
 rs=[]
 for l in (c/'traces'/f'{arm}.boundaries.tsv').read_text().splitlines():
  if l.startswith('#'): continue
  r=l.split('\t')
  if int(r[4])>345034 or (arm=='kissat' and r[2] not in names): continue
  rs.append(dict(name=names.get(r[2],r[2]),stage=r[3],conflicts=int(r[4]),decisions=int(r[5]),
   active=int(r[7]),irred=int(r[10]),red=int(r[11]),binary=int(r[12])))
 boundaries[arm]=rs
a,b=boundaries.values();assert len(a)==len(b)==204
assert [(r['name'],r['stage'],r['conflicts']) for r in a]==[(r['name'],r['stage'],r['conflicts']) for r in b]
i=next(i for i,(x,y) in enumerate(zip(a,b)) if x['active']!=y['active'] or x['irred']!=y['irred'])
assert i==199 and a[i]['conflicts']==343785 and a[i]['name']=='BVE' and a[i]['stage']=='end'
result=dict(stopped_clean=2,prior_prefix_identical=True,matching_decisions=first[0],first_decision=dict(tactix=first[1],kissat=first[2]),
 first_active_or_irred_difference=dict(index=i,tactix=a[i],kissat=b[i]),boundaries=boundaries,
 trace_sha256={p.name:sha(p) for p in (c/'traces').iterdir()},
 conclusion='BVE at 343785 creates the next persistent formula-count difference before the choice split at 345034. Extra Tactix elimination is not itself proof of weaker behavior; inspect exact candidate mechanism. Earlier transient Sweep binary differences reconverge.')
p=c/'summary.json'
if p.exists(): assert json.loads(p.read_text())==result
else: p.write_text(json.dumps(result,indent=2)+'\n')
print(result['conclusion'])
