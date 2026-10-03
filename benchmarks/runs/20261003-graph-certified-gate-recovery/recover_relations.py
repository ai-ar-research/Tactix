from pathlib import Path
import json,lzma,collections
run=Path('benchmarks/runs/20261003-graph-certified-gate-recovery')
summary=json.loads((run/'summary.json').read_text())
intervals=json.loads((run/'intervals.json').read_text());n=117
meaning={-(r*n+c+1):(r,c,c) for r in range(n) for c in range(n)}
meaning.update({d['output']:(d['row'],d['first'],d['last']) for d in intervals})
relations={};records=[];covers={};occurrence=0
for line in lzma.open(summary['input'],'rt'):
 if line.startswith(('c','p')):continue
 occurrence+=1
 c=list(map(int,line.split()))[:-1]
 if len(c)==n and all(0<x<=n*n for x in c):
  column=(c[0]-1)%n
  if set(c)=={row*n+column+1 for row in range(n)}:covers[column]=occurrence
 if len(c)!=2 or not all(x in meaning for x in c):continue
 a,b=map(meaning.get,c)
 ra,lo,hi=a;rb,bl,bh=b
 # Every literal means no selected label in its interval. Thus the binary
 # excludes simultaneously selecting any pair in their Cartesian product.
 for row,start,end,other,l,h in [(ra,lo,hi,rb,bl,bh),(rb,bl,bh,ra,lo,hi)]:
  masks=relations.setdefault((row,other),[0]*n);bits=((1<<(h-l+1))-1)<<l
  for col in range(start,end+1):masks[col]|=bits
 records.append(dict(occurrence=occurrence,clause=c,intervals=[a,b]))
fullrows=[];missing={}
for row in range(n):
 masks=relations.get((row,row),[0]*n)
 count=sum(bin((((1<<n)-1)^(1<<col)) & ~masks[col]).count("1") for col in range(n))
 if not count:fullrows.append(row)
 else:missing[row]=count
edges=[]
for (a,b),masks in relations.items():
 if a>=b:continue
 separation=0
 for d in range(n):
  if all(masks[i]&(1<<j) for i in range(n) for j in {i-d,i+d} if 0<=j<n):separation=d+1
  else:break
 if separation:edges.append(dict(a=a,b=b,separation=separation))
assert set(covers)==set(range(n))
result=dict(column_cover_occurrences=covers,permutation_consequence=(len(fullrows)==n),scope='logical composition of certified interval meanings with original binary clauses; no emitted proof or formula mutation',binary_interval_clauses=len(records),complete_row_at_most_one=len(fullrows),incomplete_row_exclusion_counts=missing,cross_row_relations=len([x for x in relations if x[0]<x[1]]),certified_separation_histogram=dict(collections.Counter(e['separation'] for e in edges)),edges=edges)
(run/'interval-relations.json').write_text(json.dumps(result,indent=2)+'\n')
print({k:v for k,v in result.items() if k not in ['edges','incomplete_row_exclusion_counts']});print('missing sample',list(missing.items())[:3])
