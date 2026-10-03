import hashlib,json,os,subprocess,sys,time
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import proof_oracle as oracle
run=Path(sys.argv[1]).resolve();index=int(os.environ['SLURM_ARRAY_TASK_ID'])
manifest=json.loads((run/'manifest.json').read_text())
for p,h in manifest['bindings'].items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,p
case=manifest['cases'][index];work=run/f'case-{index:02d}';work.mkdir()
started=time.monotonic();prefix=work/'degree'
command=[str(root/manifest['binary']),str(prefix),str(case['vertices']),str(case['degree'])]
with (work/'generator.stdout').open('w') as out,(work/'generator.stderr').open('w') as err:
 proc=subprocess.run(command,stdout=out,stderr=err,timeout=300)
result=dict(case=case,job_id=os.environ['SLURM_JOB_ID'],generator_returncode=proc.returncode,verified=False)
(run/f'result-{index:02d}.json').write_text(json.dumps(result,indent=2)+'\n')
assert proc.returncode==0
result['generation']=json.loads((work/'generator.stdout').read_text())
result['proof_bytes']=prefix.with_suffix('.dsr').stat().st_size
cnf=prefix.with_suffix('.cnf');proof=prefix.with_suffix('.dsr')
# Independently check that generated input is exactly the prescribed degree CNF.
import itertools
n=case['vertices'];d=case['degree'];edges={e:i+1 for i,e in enumerate(itertools.combinations(range(n),2))}
expected=[]
for size in (3,4):
 for vertices in itertools.combinations(range(n),size):
  expected.append(tuple((-1 if size==3 else 1)*edges[e] for e in itertools.combinations(vertices,2)))
lines=cnf.read_text().splitlines();assert lines[0]==f'p cnf {len(edges)} {len(expected)}'
actual=[tuple(map(int,line.split()[:-1])) for line in lines[1:]];assert actual==expected
checker_dir=run/'checkers';drat=str(checker_dir/'drat-trim');cake=str(checker_dir/'cake_lpr')
checkdir=work/'check';checkdir.mkdir()
check_started=time.monotonic()
ok,stage,output=oracle.check_witness_free_dsr_as_drat(drat,cake,cnf,proof,checkdir,timeout=600)
result.update(verified=ok,checker_stage=stage,checker_output=output,check_seconds=time.monotonic()-check_started)
(run/f'result-{index:02d}.json').write_text(json.dumps(result,indent=2)+'\n')
assert ok,(stage,output[-1000:])
# Supplement canonical backward checking with a forward every-lemma audit.
forward=subprocess.run([drat,str(checkdir/'checker.cnf'),str(checkdir/'proof.drat'),'-f'],capture_output=True,text=True,timeout=600)
(work/'forward.stdout').write_text(forward.stdout);(work/'forward.stderr').write_text(forward.stderr)
result['every_lemma']=oracle.proof_check_status('drat-trim',forward.returncode,forward.stdout,forward.stderr)=='verified'
assert result['every_lemma']
# Exercise the same route with a false refutation of a satisfiable unit formula.
badcnf=work/'negative.cnf';badproof=work/'negative.dsr';badcnf.write_text('p cnf 1 1\n1 0\n');badproof.write_text('0\n');baddir=work/'negative-check';baddir.mkdir()
bad,badstage,badoutput=oracle.check_witness_free_dsr_as_drat(drat,cake,badcnf,badproof,baddir,timeout=60)
result.update(negative_rejected=not bad,negative_stage=badstage,negative_output=badoutput,total_seconds=time.monotonic()-started)
assert not bad and badstage=='drat-trim'
result['hashes']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (cnf,proof)}
(run/f'result-{index:02d}.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
