"""Fixed AMO truth tables and PHP integration controls, not a fuzz campaign."""
from pathlib import Path
import importlib.util, itertools, subprocess, sys, tempfile, re
binary,root=Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve()
spec=importlib.util.spec_from_file_location('generator',root/'examples/cnf/generate.py')
g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
encodings=['pairwise','sequential','bitwise','commander','product']
# Independently enumerate existential auxiliary assignments for each AMO.
for encoding in encodings:
 for n in range(2,6):
  clauses,end=g.at_most_one(list(range(1,n+1)),n+1,encoding)
  for inputs in itertools.product((False,True),repeat=n):
   exists=False
   for auxiliary in itertools.product((False,True),repeat=end-1-n):
    values=inputs+auxiliary
    if all(any(values[abs(x)-1] == (x>0) for x in c) for c in clauses):
     exists=True;break
   assert exists == (sum(inputs)<=1),(encoding,n,inputs)

def dimacs(n,clauses):
 return f'p cnf {n} {len(clauses)}\n'+''.join(' '.join(map(str,c+[0]))+'\n' for c in clauses)
with tempfile.TemporaryDirectory(prefix='php-amo-fixed-',dir=Path.cwd()) as tmp:
 directory=Path(tmp)
 for encoding in encodings:
  for holes in (2,3,5):
   for functional in (False,True):
    n,clauses=g.pigeonhole(holes+1,holes,encoding,functional)
    # A deterministic variable reversal and clause/literal permutation.
    variants=[clauses, [[(n+1-abs(x))*(1 if x>0 else -1) for x in reversed(c)] for c in reversed(clauses)]]
    for index,variant in enumerate(variants):
     path=directory/'input.cnf';path.write_text(dimacs(n,variant))
     command=[binary,path]
     proof=directory/"output.pbp"
     emit_proof=holes==3 and not functional and index==0
     if emit_proof: command.append(proof)
     run=subprocess.run(command,text=True,capture_output=True,timeout=30)
     if emit_proof:
      text=proof.read_text()
      assert "conclusion UNSAT" in text and "end pseudo-Boolean proof" in text
     assert run.returncode==20,(encoding,holes,functional,index,run.stdout,run.stderr)
     assert 'c php-er decisions=0' in run.stdout
     if encoding!='pairwise' and not (encoding=='commander' and holes==2):
      count=int(run.stdout.split('normalized-collisions=')[1].split()[0])
      assert count>0,(encoding,run.stdout)
  # The same construction with as many pigeons as holes is satisfiable.
  n,clauses=g.pigeonhole(3,3,encoding)
  path=directory/'sat.cnf';path.write_text(dimacs(n,clauses))
  run=subprocess.run([binary,path],text=True,capture_output=True,timeout=30)
  assert run.returncode==1 and 's UNSATISFIABLE' not in run.stdout
 # Unused declared original identities must remain outside the extension
 # namespace, even though they do not occur in the recognized PHP core.
 n,clauses=g.pigeonhole(4,3)
 declared=n+11
 path=directory/'namespace.cnf';path.write_text(dimacs(declared,clauses))
 proof=directory/'namespace.pbp'
 run=subprocess.run([binary,path,proof],text=True,capture_output=True,timeout=30)
 assert run.returncode==20,(run.stdout,run.stderr)
 pivot=re.search(r' : x(\d+) ->',proof.read_text())
 assert pivot and int(pivot[1])==declared+1,(pivot[1] if pivot else None,declared)
 # Missing the entire capacity constraint of one hole gives an explicit SAT
 # witness: put every pigeon in that unconstrained hole.
 for encoding in encodings:
  holes=3; pigeons=4; next_var=pigeons*holes+1
  clauses=[[p*holes+h+1 for h in range(holes)] for p in range(pigeons)]
  for h in (1,2):
   extra,next_var=g.at_most_one([p*holes+h+1 for p in range(pigeons)],next_var,encoding)
   clauses.extend(extra)
  path=directory/'damaged.cnf';path.write_text(dimacs(next_var-1,clauses))
  run=subprocess.run([binary,path],text=True,capture_output=True,timeout=30)
  assert run.returncode==1 and 's UNSATISFIABLE' not in run.stdout,(encoding,run.stdout)
print('Five AMO encodings: exact auxiliary truth tables, 60 PHP variants, ten SAT controls')
