"""Deliberately simple all-addition RUP/RAT elaborator for fixed microcontrols.
Independent CakeLPR, not this script, is the acceptance authority.
"""
from pathlib import Path
import sys
cnf,proof,out=map(Path,sys.argv[1:]);db={}
for line in cnf.read_text().splitlines():
 if line and line[0] not in 'cp':db[len(db)+1]=list(map(int,line.split()[:-1]))
def rup(candidate):
 assignment={};hints=[]
 for lit in candidate:
  v=abs(lit);value=lit<0
  if v in assignment and assignment[v]!=value:return []
  assignment[v]=value
 while True:
  progress=False
  for idx,c in db.items():
   pending=[];satisfied=False
   for lit in c:
    val=assignment.get(abs(lit))
    if val is None:pending.append(lit)
    elif val==(lit>0):satisfied=True;break
   if satisfied:continue
   if not pending:return hints+[idx]
   if len(pending)==1:
    lit=pending[0];assignment[abs(lit)]=lit>0;hints.append(idx);progress=True
  if not progress:return None
lines=[]
next_id=len(db)+1
for line in proof.read_text().splitlines():
 if line.startswith('d '):
  target=sorted(map(int,line.split()[1:-1])); found=next((i for i,c in db.items() if sorted(c)==target),None)
  assert found is not None,target
  del db[found];lines.append(f'{next_id-1} d {found} 0');continue
 c=list(map(int,line.split()[:-1]));hints=rup(c)
 if hints is None:
  assert c
  pivot=c[0];hints=[]
  for idx,parent in db.items():
   if -pivot not in parent:continue
   resolvent=list(dict.fromkeys([x for x in c if x!=pivot]+[x for x in parent if x!=-pivot]))
   if any(-x in resolvent for x in resolvent):
    hints.append(-idx);continue
   chain=rup(resolvent);assert chain is not None,(len(db)+1,idx)
   hints.extend([-idx]+chain)
 idx=next_id;next_id+=1;db[idx]=c
 if c not in [[13],[]]: assert not any(abs(h) in range(9,13) for h in hints), (idx,c,hints)
 lines.append(' '.join(map(str,[idx]+c+[0]+hints+[0])))
out.write_text('\n'.join(lines)+'\n');print(len(lines),'all-addition LRAT lines')
