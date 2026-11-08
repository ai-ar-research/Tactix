"""Compare compiled streaming pairwise generation to the existing DIMACS oracle."""
from pathlib import Path
import importlib.util,subprocess,sys
binary,root=Path(sys.argv[1]),Path(sys.argv[2])
spec=importlib.util.spec_from_file_location('generator',root/'examples/cnf/generate.py')
g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
for pigeons,holes in [(2,1),(3,2),(4,3),(7,6),(1,7)]:
 n,clauses=g.pigeonhole(pigeons,holes)
 expected=f'c Pigeonhole principle: {pigeons} pigeons, {holes} holes.\np cnf {n} {len(clauses)}\n'+''.join(' '.join(map(str,c+[0]))+'\n' for c in clauses)
 run=subprocess.run([binary,str(pigeons),str(holes)],text=True,capture_output=True,check=True)
 assert run.stdout==expected
for args in [('0','3'),('-1','3'),('x','3'),('3x','2'),('2147483647','2147483647')]:
 assert subprocess.run([binary,*args],capture_output=True).returncode==1
print('Streaming generator matches five DIMACS fixtures; invalid dimensions rejected')
