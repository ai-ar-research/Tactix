#!/usr/bin/env python3
"""Retain diagnostic neutrality and the Congruence -> Vivify -> choice chain."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
RUNS=ROOT/'benchmarks/runs'
ARMS=('tactix','kissat')
TARGET='-21187,-21186,18559,21214'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''): h.update(chunk)
 return h.hexdigest()
def rows(p):
 with p.open() as f:
  for l in f:
   if not l.startswith('#'): yield l.rstrip('\n').split('\t')
def payload(r): return dict(x.split('=',1) for x in r[-1].split('|') if '=' in x)
def campaign(number): return next(RUNS.glob('20260907-'+str(number)+'-*'))
def validate(n,job,parent=None):
 c=campaign(n);m=json.loads((c/'manifest.json').read_text())
 for rel,expected in m['hashes'].items(): assert sha(c/rel)==expected,rel
 for i,arm in enumerate(ARMS):
  log=(c/'logs'/f'{job}_{i}.log').read_text()
  assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
  if parent:
   for suffix in ('decisions','prefix','state','queue','bumps'):
    assert sha(c/'traces'/f'{arm}.{suffix}.tsv')==sha(campaign(parent)/'traces'/f'{arm}.{suffix}.tsv'),(n,arm,suffix)
 return c,dict(stopped_clean=2,previous_standalone_streams_identical=bool(parent),
  trace_sha256={p.name:sha(p) for p in (c/'traces').iterdir()})
def save(c,s):
 p=c/'summary.json'
 if p.exists(): assert json.loads(p.read_text())==s
 else: p.write_text(json.dumps(s,indent=2)+'\n')
 print(c.name,s.get('conclusion','PASS'))
def boundary(c,arm):
 names={'congruence':'Congruence','substitute_early':'Substitute','substitute_late':'Substitute',
  'backbone_early':'Backbone','backbone_late':'Backbone','vivify':'Vivify','sweep':'Sweep',
  'transitive':'TransitiveReduce','factor':'Factor'}
 return [dict(name=names.get(r[2],r[2]),stage=r[3],conflicts=int(r[4]),
  propagations=int(r[6]),irred=int(r[10]),red=int(r[11]),binary=int(r[12]))
  for r in rows(c/'traces'/f'{arm}.boundaries.tsv') if r[2]!='probe']
def counts(c,arm):
 return [dict(x.split(':') for x in r[3].split(',')) for r in rows(c/'traces'/f'{arm}.counts.tsv')]

c,s=validate(5364,1668818,5363)
witness={}
for arm in ARMS:
 found=[]
 for r in rows(c/'traces'/f'{arm}.events.tsv'):
  assert r[2]!='trace_truncated'
  if r[2]=='watch' and r[3]=='132831' and payload(r).get('clause')==TARGET:
   found.append(dict(event=r[2],conflicts=r[3],decisions=r[4],payload=payload(r)))
 witness[arm]=found
assert witness['tactix'] and witness['kissat']
s.update(witness=witness,conclusion='Target clause is scanned under 21186 then 21187 in Tactix; native assigns 21214 during the 21186 scan. Watch watermark 132831 precedes implication watermark 132832. Pre-scan snapshots are not per-entry execution logs.')
save(c,s)

c,s=validate(5365,1668820,5364)
groups={};watch={}
for arm in ARMS:
 gs=[];ws=[]
 for r in rows(c/'traces'/f'{arm}.events.tsv'):
  assert r[2]!='trace_truncated'
  d=payload(r)
  if r[2] in ('watch','watch_post') and d.get('clause')==TARGET:
   ws.append([r[2],r[3],r[4],r[6],{k:v for k,v in d.items() if k not in ('id','order','garbage')}])
  if r[3]=='132816' and r[2]=='vivify_candidate':
   if d['ordinal']=='0': gs.append(dict(begin=int(d['ticks']),limit=int(d['limit']),candidates=[]))
   gs[-1]['candidates'].append({k:d[k] for k in ('ordinal','clause','pose','decisions','size','glue','priority','ticks')})
  elif r[3]=='132816' and r[2]=='vivify_candidate_end': gs[-1]['end']=int(d['ticks'])
 groups[arm]=gs;watch[arm]=ws
assert all(len(g)==8 for g in groups.values())
i=next(i for i,(a,b) in enumerate(zip(watch['tactix'],watch['kissat'])) if a!=b)
assert i==3656,i
schedule=[]
for g in range(8):
 a=groups['tactix'][g];b=groups['kissat'][g]
 identity=lambda d:{k:v for k,v in d.items() if k!='ticks'}
 j=next((j for j,(x,y) in enumerate(zip(a['candidates'],b['candidates'])) if identity(x)!=identity(y)),None)
 if g<4: assert j is None and len(a['candidates'])==len(b['candidates'])
 if g==4: assert j==2915,j
 schedule.append(dict(group=g,first_identity_difference=j,
  arms={arm:dict(begin=groups[arm][g]['begin'],limit=groups[arm][g]['limit'],
   allowance=groups[arm][g]['limit']-groups[arm][g]['begin'],
   used=groups[arm][g]['end']-groups[arm][g]['begin'],count=len(groups[arm][g]['candidates']),
   witness=groups[arm][g]['candidates'][max(0,j-1):j+2] if j is not None else []) for arm in ARMS}))
s.update(matching_target_watch_records=i,target_watch_context={arm:w[i-1:i+2] for arm,w in watch.items()},
 vivify_groups=schedule,conclusion='First four Vivify candidate schedules match; second-dose tier1 swaps candidates at ordinal 2915. Unequal work/budget carry stops native tier2 two candidates earlier, changing later target pose. Do not patch a cursor to compensate.')
save(c,s)

c,s=validate(5366,1668822,5365)
a=counts(c,'tactix');b=counts(c,'kissat')
assert a[:4]==b[:4] and a[4]['-50']=='169' and b[4]['-50']=='160'
s.update(counts=dict(tactix=a,kissat=b),conclusion='First four selected count vectors match; second dose differs: -50 is 169/160 and -2116 is 13/11. This count image changes candidate ordering.')
save(c,s)

c,s=validate(5367,1668824,5366)
assert not list((c/'traces').glob('*.bve.tsv'))
s.update(bve_observed=False,reference_source='kissat/src/probe.c:kissat_probe has no ordinary BVE slot',
 conclusion='No ordinary BVE candidate event at 132816 in either arm. Expected diagnostic absence, not solver failure; inspect actual Probe slots.')
save(c,s)

c,s=validate(5368,1668826,5366)
a=boundary(c,'tactix');b=boundary(c,'kissat')
assert len(a)==len(b)==36
assert [(r['name'],r['stage']) for r in a]==[(r['name'],r['stage']) for r in b]
i=next(i for i,(x,y) in enumerate(zip(a,b)) if x['red']!=y['red'])
assert i==19 and a[i]['red']-b[i]['red']==16
s.update(boundaries=dict(tactix=a,kissat=b),first_red_difference=i,
 conclusion='Second Congruence leaves 16 extra redundant clauses, next Substitute removes six. Earlier transient Sweep binary count 1832/1834 reconverges after Substitute and remains a separate producer lead.')
save(c,s)

c,s=validate(5369,1668828)
for suffix in ('decisions','prefix','state','queue','bumps','counts','boundaries'):
 assert sha(c/'traces'/f'kissat.{suffix}.tsv')==sha(campaign(5368)/'traces'/f'kissat.{suffix}.tsv')
a=boundary(c,'tactix');b=boundary(c,'kissat')
assert all(x['red']==y['red'] and x['irred']==y['irred'] for x,y in zip(a,b))
assert counts(c,'tactix')==counts(c,'kissat')
assert sha(c/'traces/tactix.decisions.tsv')==sha(c/'traces/kissat.decisions.tsv')
n=sum(1 for _ in rows(c/'traces/tactix.decisions.tsv'))
assert n==328670
bumps={arm:list(rows(c/'traces'/f'{arm}.bumps.tsv')) for arm in ARMS}
assert [r[3] for r in bumps['tactix']]==[r[3] for r in bumps['kissat']]
s.update(native_prior_streams_identical=True,matching_decisions=n,matching_selected_vivify_vectors=8,
 matching_queue_bump_identities=len(bumps['tactix']),boundaries=dict(tactix=a,kissat=b),
 conclusion='Isolated prefix repair closes all observed redundant counts, eight selected Vivify count vectors, queue bump identities, and all 328670 decisions through conflict 133243. UNFUZZED; no runtime claim. Transient Sweep binary discrepancy is not closed.')
save(c,s)
