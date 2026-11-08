"""Integration regression over fixed PHP encodings; no benchmark generators."""
from pathlib import Path
import itertools,re,subprocess,sys
from tempdir_cleanup import RetryingTemporaryDirectory
binary,root=map(lambda p:Path(p).resolve(),sys.argv[1:])
encodings=['pairwise','sequential','bitwise','commander','product']
def read(path):
 clauses=[];pending=[];variables=0
 for line in path.read_text().splitlines():
  if not line or line.startswith('c'):continue
  if line.startswith('p'):variables=int(line.split()[2]);continue
  for literal in map(int,line.split()):
   if literal:pending.append(literal)
   else:clauses.append(pending);pending=[]
 return variables,clauses
def text(n,clauses):return f'p cnf {n} {len(clauses)}\n'+''.join(' '.join(map(str,c+[0]))+'\n' for c in clauses)
def run(path,*options,expected=20):
 result=subprocess.run([binary,path,*options],capture_output=True,text=True,timeout=15)
 assert result.returncode==expected,(path,result.returncode,result.stdout,result.stderr)
 return result
with RetryingTemporaryDirectory(prefix='php-encodings-',dir=Path.cwd()) as temporary:
 work=Path(temporary)
 for encoding in encodings:
  # Three fixed dimensions, two ordering/numbering variants. Functional row
  # constraints are a test mutation of the fixture, not a shipped generator.
  for holes in (2,3,5):
   n,clauses=read(root/f'examples/cnf/php/{holes+1}-{holes}-{encoding}.cnf')
   rows=[c for c in clauses if len(c)==holes and min(c)>0]
   for functional in (False,True):
    formula=clauses+[[-a,-b] for row in rows for a,b in itertools.combinations(row,2)] if functional else clauses
    for permuted in (False,True):
     variant=[[(n+1-abs(l))*(1 if l>0 else -1) for l in reversed(c)] for c in reversed(formula)] if permuted else formula
     path=work/f'{encoding}-{holes}-{functional}-{permuted}.cnf';path.write_text(text(n,variant))
     result=run(path,'--prioritize')
     assert f'definitions={holes+1}' in result.stdout
   if holes==3:
    path=work/f'proof-{encoding}.cnf';path.write_text(text(n,clauses))
    proof=work/f'{encoding}.pbp';run(path,proof)
    assert 'conclusion UNSAT' in proof.read_text()
    # Removing one positive pigeon row has an explicit SAT completion.
    sat=clauses.copy();sat.remove(rows[-1]);path=work/f'sat-{encoding}.cnf';path.write_text(text(n,sat));run(path,expected=1)
 # Namespace holes belong to the input, never to new ER variables.
 n,clauses=read(root/'examples/cnf/php/4-3-pairwise.cnf')
 path=work/'namespace.cnf';path.write_text(text(n+11,clauses));proof=work/'namespace.pbp';run(path,proof)
 pivot=re.search(r' : x(\d+) ->',proof.read_text());assert pivot and int(pivot[1])==n+12
print('60 fixed PHP variants, five SAT controls and five proof-output cases')
