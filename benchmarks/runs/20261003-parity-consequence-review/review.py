from pathlib import Path
import itertools,json,subprocess,sys,hashlib
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tools/bench'));import proof_oracle as oracle
# x+a+b=0, b+c+d=0, d+e+a=0, y+c+e=0 imply x=y.
scopes=[(1,3,4),(4,5,6),(6,7,3),(2,5,7)]
clauses=[]
for scope in scopes:
 for vals in itertools.product([0,1],repeat=3):
  if sum(vals)%2:clauses.append(tuple(-v if value else v for v,value in zip(scope,vals)))
models=[]
for bits in itertools.product([False,True],repeat=7):
 if all(any(bits[abs(x)-1]==(x>0) for x in c) for c in clauses):
  assert bits[0]==bits[1];models.append(bits)
assert len(models)==8
# Four-clause complete XOR is required for inference. Three odd-forbidding
# clauses omit one odd assignment, which is a countermodel to asserting XOR=0.
partial=clauses[:3];countermodels=[bits for bits in itertools.product([False,True],repeat=7) if all(any(bits[abs(x)-1]==(x>0) for x in c) for c in partial) and sum(bits[v-1] for v in scopes[0])%2]
assert countermodels
# Unpropagated, disjoint closing gadget; no prefix hint may reference it.
gadget=[(8,9),(8,-9),(-8,9),(-8,-9)];db=clauses+gadget
work={i+1:tuple(c) for i,c in enumerate(clauses)};known={tuple(sorted(c)):i+1 for i,c in enumerate(db)}
proof=[];lrat=[];records=[]
for pivot in range(3,8):
 pos=[(i,c) for i,c in work.items() if pivot in c];neg=[(i,c) for i,c in work.items() if -pivot in c]
 next_work={i:c for i,c in work.items() if pivot not in c and -pivot not in c}
 for pi,pc in pos:
  for ni,nc in neg:
   cl=tuple(sorted(set(x for x in pc if x!=pivot)|set(x for x in nc if x!=-pivot)))
   if any(-x in cl for x in cl):continue
   if cl not in known:
    idx=len(db)+len(proof)+1;known[cl]=idx;proof.append(cl)
    lrat.append([idx,*cl,0,pi,ni,0]);assert pi not in range(17,21) and ni not in range(17,21)
    records.append(dict(id=idx,clause=cl,rule='resolution',pivot=pivot,parents=[pi,ni],meaning='Intermediate XOR-system consequence'))
   next_work[known[cl]]=cl
 work=next_work
assert {tuple(c) for c in work.values()}=={(-1,2),(-2,1)}
for r in records:
 if r['clause'] in [(-1,2),(-2,1)]:r['meaning']='One direction of x iff y derived by cancellation of four XOR equations'
unit_id=len(db)+len(proof)+1;lrat.extend([[unit_id,8,0,17,18,0],[unit_id+1,0,unit_id,19,20,0]]);proof.extend([(8,),()])
cnf=run/'audit.cnf';dsr=run/'consequence.dsr';lr=run/'all-addition.lrat'
cnf.write_text(f'p cnf 9 {len(db)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in db))
dsr.write_text(''.join(' '.join(map(str,c))+(' ' if c else '')+'0\n' for c in proof))
lr.write_text(''.join(' '.join(map(str,line))+'\n' for line in lrat))
(run/'provenance.json').write_text(json.dumps(dict(equations=scopes,rhs=0,records=records,gadget_clause_ids=list(range(17,21))),indent=2)+'\n')
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers';check=run/'check';check.mkdir()
ok,stage,out=oracle.check_witness_free_dsr_as_drat(str(checkers/'drat-trim'),str(checkers/'cake_lpr'),cnf,dsr,check,timeout=30);assert ok,(stage,out)
commands=[('forward',[str(checkers/'drat-trim'),str(cnf),str(check/'proof.drat'),'-f'],'drat-trim'),('all-addition',[str(checkers/'cake_lpr'),str(cnf),str(lr)],'cake_lpr')]
for name,cmd,kind in commands:
 p=subprocess.run(cmd,capture_output=True,text=True,timeout=30);(run/(name+'.stdout')).write_text(p.stdout);(run/(name+'.stderr')).write_text(p.stderr)
 assert oracle.proof_check_status(kind,p.returncode,p.stdout,p.stderr)=='verified'
# A deliberately unjustified x unit as the first LRAT addition must fail.
bad=run/'invalid.lrat';bad.write_text('21 1 0 0\n');p=subprocess.run([str(checkers/'cake_lpr'),str(cnf),str(bad)],capture_output=True,text=True,timeout=30)
(run/'negative.stdout').write_text(p.stdout);(run/'negative.stderr').write_text(p.stderr)
assert oracle.proof_check_status('cake_lpr',p.returncode,p.stdout,p.stderr)=='rejected'
result=dict(scope='Fixed parity consequence proof, not production implementation or a target result',models=len(models),derived_binary_equivalence=[[-1,2],[-2,1]],prefix_additions=len(records),proof_bytes=dsr.stat().st_size,canonical_verified=True,all_addition_cake_verified=True,forward_drat_verified=True,negative_rejected=True,incomplete_xor_countermodel=[int(x) for x in countermodels[0]],checkers={str(checkers/n):hashlib.sha256((checkers/n).read_bytes()).hexdigest() for n in ['drat-trim','cake_lpr']})
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
