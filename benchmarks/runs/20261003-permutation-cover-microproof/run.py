from pathlib import Path
import hashlib,itertools,json,subprocess,sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
import proof_oracle as oracle
run=Path(__file__).resolve().parent
pin=root/'build/pin-permutation-cover-dsr-72cbb8da-01'
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
result=[]
for n in [2,3,4,6]:
 work=run/f'n{n}';work.mkdir();prefix=work/'matrix'
 proc=subprocess.run([str(pin/'probe'),str(prefix),str(n)],capture_output=True,text=True,timeout=30)
 (work/'generator.stdout').write_text(proc.stdout);(work/'generator.stderr').write_text(proc.stderr)
 assert proc.returncode==0,(n,proc.stderr)
 cnf=prefix.with_suffix('.cnf');proof=prefix.with_suffix('.dsr')
 expected=[]
 for r in range(n):
  if not r:expected.extend([[-(c+1)] for c in range(n)])
  expected.extend([[-(r*n+a+1),-(r*n+b+1)] for a,b in itertools.combinations(range(n),2)])
 expected.extend([[r*n+c+1 for r in range(n)] for c in range(n)])
 lines=cnf.read_text().splitlines();assert lines[0]==f'p cnf {n*n} {len(expected)}'
 assert [list(map(int,line.split()[:-1])) for line in lines[1:]]==expected
 check=work/'check';check.mkdir()
 ok,stage,output=oracle.check_witness_free_dsr_as_drat(str(checkers/'drat-trim'),str(checkers/'cake_lpr'),cnf,proof,check,timeout=30)
 (work/'canonical-result.json').write_text(json.dumps(dict(verified=ok,stage=stage,output=output),indent=2)+'\n');assert ok,(stage,output)
 forward=subprocess.run([str(checkers/'drat-trim'),str(check/'checker.cnf'),str(check/'proof.drat'),'-f'],capture_output=True,text=True,timeout=30)
 (work/'forward.stdout').write_text(forward.stdout);(work/'forward.stderr').write_text(forward.stderr)
 assert oracle.proof_check_status('drat-trim',forward.returncode,forward.stdout,forward.stderr)=='verified'
 result.append(dict(n=n,generation=json.loads(proc.stdout),proof_bytes=proof.stat().st_size,canonical_verified=True,every_lemma_verified=True,cnf_sha256=hashlib.sha256(cnf.read_bytes()).hexdigest(),proof_sha256=hashlib.sha256(proof.read_bytes()).hexdigest()))
(run/'result.json').write_text(json.dumps(dict(scope='Fixed microproofs under explicit negated row cover; not a target proof or unconditioned inprocessing derivation',cases=result),indent=2)+'\n')
print(json.dumps(result,indent=2))
