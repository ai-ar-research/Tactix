"""Audit lifted nonterminal proof using a disjoint, initially non-UP gadget.
Canonical core verification alone is insufficient: require forward every lemma.
"""
from pathlib import Path
import json,subprocess,sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
import proof_oracle as oracle
run=Path(__file__).resolve().parent
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
results=[]
for n in [3,4]:
 source=run/f'n{n}';work=run/f'lifted-certified-n{n}';work.mkdir()
 original=[list(map(int,l.split()[:-1])) for l in (source/'matrix.cnf').read_text().splitlines()[1:]]
 removed=[c for c in original if len(c)==1];assert removed==[[-i] for i in range(1,n+1)]
 original=[c for c in original if len(c)>1];proof=[];mapping=[]
 for line_id,line in enumerate((source/'matrix.dsr').read_text().splitlines(),1):
  cl=list(map(int,line.split()[:-1]));assert not any(x==0 for x in cl)
  lifted=list(dict.fromkeys(cl+list(range(1,n+1))))
  if any(-x in lifted for x in lifted):continue
  proof.append(lifted);mapping.append(dict(source_line=line_id,output_line=len(proof),clause=lifted))
 assert proof[-1]==list(range(1,n+1))
 maximum=max(abs(x) for c in original+proof for x in c);a,b=maximum+1,maximum+2
 gadget=[[a,b],[a,-b],[-a,b],[-a,-b]]
 assert all(len(c)==2 for c in gadget) and all(abs(x)>maximum for c in gadget for x in c)
 # No initial units in either original matrix or gadget. While checking any
 # prefix RUP/RAT obligation, no gadget variable is assumed or propagated.
 assert all(len(c)>1 for c in original)
 def writecnf(path,cs):path.write_text(f'p cnf {b} {len(cs)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in cs))
 def writeproof(path,cs):path.write_text(''.join(' '.join(map(str,c))+(' ' if c else '')+'0\n' for c in cs))
 cnf=work/'audit.cnf';dsr=work/'audit.dsr';writecnf(cnf,original+gadget);writeproof(dsr,proof+[[a],[]])
 (work/'provenance.json').write_text(json.dumps(dict(goal=proof[-1],conditioning_literals=list(range(1,n+1)),gadget_variables=[a,b],prefix_line_map=mapping),indent=2)+'\n')
 check=work/'check';check.mkdir();ok,stage,out=oracle.check_witness_free_dsr_as_drat(str(checkers/'drat-trim'),str(checkers/'cake_lpr'),cnf,dsr,check,timeout=30)
 assert ok,(stage,out)
 cmd=[str(checkers/'drat-trim'),str(check/'checker.cnf'),str(check/'proof.drat'),'-f']
 p=subprocess.run(cmd,capture_output=True,text=True,timeout=30);(work/'forward.stdout').write_text(p.stdout);(work/'forward.stderr').write_text(p.stderr)
 assert oracle.proof_check_status('drat-trim',p.returncode,p.stdout,p.stderr)=='verified'
 subprocess.run([sys.executable,str(run/'elaborate_micro_lrat.py'),str(cnf),str(dsr),str(work/'all-addition.lrat')],check=True,timeout=30)
 gadget_ids=set(range(len(original)+1,len(original)+5))
 for line in (work/'all-addition.lrat').read_text().splitlines()[:len(proof)]:
  fields=list(map(int,line.split()));end=fields.index(0)
  assert not ({abs(x) for x in fields[end+1:-1]} & gadget_ids)
 p=subprocess.run([str(checkers/'cake_lpr'),str(cnf),str(work/'all-addition.lrat')],capture_output=True,text=True,timeout=30)
 (work/'cake-forward.stdout').write_text(p.stdout);(work/'cake-forward.stderr').write_text(p.stderr)
 assert oracle.proof_check_status('cake_lpr',p.returncode,p.stdout,p.stderr)=='verified',(p.returncode,p.stdout,p.stderr)
 # An invalid early empty clause cannot exploit this unpropagated gadget.
 negative=work/'negative.drat';writeproof(negative,[[]]+proof+[[a],[]])
 p=subprocess.run([str(checkers/'drat-trim'),str(cnf),str(negative),'-f'],capture_output=True,text=True,timeout=30)
 (work/'negative.stdout').write_text(p.stdout);(work/'negative.stderr').write_text(p.stderr)
 assert p.returncode>=0 and 'NOT VERIFIED' in p.stdout and 'VERIFIED'!=p.stdout.strip()
 results.append(dict(n=n,prefix_lines=len(proof),proof_bytes=dsr.stat().st_size,canonical_verified=ok,forward_drat_verified=True,forward_cake_verified=True,negative_returncode=p.returncode,negative_semantically_rejected=True))
(run/'lifted-audit-result.json').write_text(json.dumps(dict(scope='Nonterminal prefix audit with disjoint unit-free gadget; no target proof or production acceptance',cases=results),indent=2)+'\n');print(json.dumps(results,indent=2))
