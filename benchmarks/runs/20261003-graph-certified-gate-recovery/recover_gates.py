"""Static certified local-gate recovery; does not run a solver or alter CNF."""
import collections, hashlib, json, lzma, sys
from pathlib import Path
source, destination = map(Path, sys.argv[1:])
raw=lzma.open(source,'rb').read(); clauses=[]; pending=[]
for line in raw.decode().splitlines():
    if not line or line[0] in 'cp':continue
    for token in line.split():
        lit=int(token)
        if lit:pending.append(lit)
        else:clauses.append(tuple(pending));pending=[]
assert not pending
index={tuple(sorted(c)):i+1 for i,c in enumerate(clauses)}
def lookup(c):return index.get(tuple(sorted(c)))
def rup_chain(candidate, parents):
    assigned={abs(x):x<0 for x in candidate}
    for occurrence in parents:
        c=clauses[occurrence-1]
        if any(abs(x) in assigned and assigned[abs(x)]==(x>0) for x in c):return False
        unset=[x for x in c if abs(x) not in assigned]
        if not unset:return True
        if len(unset)!=1:return False
        assigned[abs(unset[0])]=unset[0]>0
    return False
records=[]
for occurrence,c in enumerate(clauses,1):
    if len(c)!=3 or len(set(map(abs,c)))!=3:continue
    z=max(c,key=abs)
    if z<0:continue
    a,b=[x for x in c if x!=z]
    explanations=[]
    for x,other in [(a,b),(b,a)]:
        target=(-x,-z);direct=lookup(target)
        if direct:parents=[direct]
        else:
            last=lookup((other,-x,-z))
            first=lookup((-other,-x)) or lookup((-other,-z))
            if not first or not last:break
            parents=[first,last]
        assert rup_chain(target,parents)
        explanations.append(dict(clause=list(target),parents=parents,already_explicit=bool(direct)))
    if len(explanations)==2:
        records.append(dict(output=z,inputs=[a,b],defining_clause=occurrence,implications=explanations))
# Diagnostic interpretation for this indexed input; gate proofs above are layout-neutral.
width=117;primary=width*width
absent={};intervals=[]
for r in sorted(records,key=lambda r:(r['output'],r['defining_clause'])):
    parts=[]
    for x in r['inputs']:
        if 0<x<=primary:parts.append(frozenset([x]))
        elif x<0 and -x in absent:parts.append(absent[-x])
        else:break
    if len(parts)!=2:continue
    support=parts[0]|parts[1]
    if r['output'] in absent:
        assert absent[r['output']]==support
    absent[r['output']]=support
    rows={(v-1)//width for v in support};columns={(v-1)%width for v in support}
    if len(rows)==1 and len(columns)==max(columns)-min(columns)+1:
        intervals.append(dict(output=r['output'],row=next(iter(rows)),first=min(columns),last=max(columns),defining_clause=r['defining_clause']))
by_support=collections.defaultdict(list)
for z,support in absent.items():by_support[tuple(sorted(support))].append(z)
result=dict(scope='static original-CNF recovery, no added constraints or solver run',input=str(source),input_sha256=hashlib.sha256(raw).hexdigest(),gates=len(records),unique_outputs=len({r['output'] for r in records}),missing_implication_records=sum(not e['already_explicit'] for r in records for e in r['implications']),primary_absence_definitions=len(absent),row_interval_definitions=len(intervals),duplicate_support_groups=sum(len(v)>1 for v in by_support.values()),interval_widths=dict(collections.Counter(r['last']-r['first']+1 for r in intervals)))
destination.mkdir(parents=True,exist_ok=False)
(destination/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
with (destination/'gate-provenance.jsonl').open('w') as f:
    for r in records:f.write(json.dumps(r)+'\n')
(destination/'intervals.json').write_text(json.dumps(intervals)+'\n')
print(json.dumps(result,indent=2))
