from pathlib import Path
from collections import Counter
import json
R=Path(__file__).resolve().parent;P=R/'pins';O=R/'results'
def clauses(p):
 out=[];a=[]
 for line in p.read_text().splitlines():
  if not line or line.startswith(('c','p')):continue
  for n in map(int,line.split()):
   if n:a.append(n)
   else:out.append(tuple(a));a=[]
 assert not a
 return out
def roots(cs):
 fixed=set();changed=True
 while changed:
  changed=False
  for c in cs:
   if any(l in fixed for l in c):continue
   left=[l for l in c if -l not in fixed]
   assert left
   if len(left)==1 and left[0] not in fixed:
    fixed.add(left[0]);changed=True
 return fixed
def normalized(cs,fixed):
 out=[]
 for c in cs:
  if any(l in fixed for l in c):continue
  live=tuple(sorted(set(l for l in c if -l not in fixed)))
  assert live
  out.append(live)
 return Counter(out+[(l,) for l in fixed])
def snap(arm,cnf):
 fixed=roots(clauses(P/cnf));boundary=None
 for line in (O/(arm+'.tsv')).read_text().splitlines():
  if line.startswith('#'):continue
  row=line.split('\t')
  if len(row)<22:continue
  if row[2]=='root_assign':fixed.add(int(row[21]))
  if row[2]=='preprocess_begin':boundary=row;break
 assert boundary and len(fixed)==int(boundary[10]),(arm,len(fixed),boundary)
 cs=[]
 for line in (O/(arm+'.large.tsv')).read_text().splitlines():
  if line.startswith('#'):continue
  row=line.split('\t')
  if row[0]=='0':cs.append(tuple(map(int,row[3].split(','))))
 for line in (O/(arm+'.binary.tsv')).read_text().splitlines():
  if line.startswith('#'):continue
  row=line.split('\t');cs.append((int(row[1]),int(row[2])))
 q=[int(line.split('\t')[1]) for line in (O/(arm+'.queue.tsv')).read_text().splitlines() if not line.startswith('#')]
 return normalized(cs,fixed),fixed,q
ref=normalized(clauses(P/'residual.cnf'),roots(clauses(P/'residual.cnf')))
result={};data={}
for arm,cnf in [('integrated','original.cnf'),('fresh-residual','residual.cnf')]:
 s,f,q=snap(arm,cnf);data[arm]=(s,f,q)
 result[arm]={'canonical_clause_count':sum(s.values()),'root_count':len(f),'root_units':sorted(f),'queue_size':len(q),'queue':q,'equal_reference_multiset':s==ref,'only_snapshot_count':sum((s-ref).values()),'only_reference_count':sum((ref-s).values()),'only_snapshot_sample':list((s-ref).elements())[:20],'only_reference_sample':list((ref-s).elements())[:20]}
a,b=data['integrated'][2],data['fresh-residual'][2];i=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
result['comparison']={'formula_multiset_equal':data['integrated'][0]==data['fresh-residual'][0],'root_set_equal':data['integrated'][1]==data['fresh-residual'][1],'first_queue_difference':i,'integrated_at_difference':a[i:i+10],'fresh_at_difference':b[i:i+10]}
(O/'comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({a:{k:v for k,v in d.items() if k not in ('queue','root_units')} for a,d in result.items()},indent=2))
